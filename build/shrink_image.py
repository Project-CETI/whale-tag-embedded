#!/usr/bin/env python3
"""
shrinker: reduce the size of a Raspberry PI SD card image.
"""

import argparse
import glob
import os
import stat
import subprocess
import sys

from imgtools import (
    SECTOR_BYTES,
    LoopDev,
    Mount,
    get_partition_info,
    resize2fs,
    resize_part,
)


def log(args, *msg):
    if args.verbose:
        print(*msg)


def do_resize(args):
    print("Creating loopback device...")
    with LoopDev(args, args.image) as disk_dev:
        print("Reading partition info...")
        start_sector = get_partition_info(args, disk_dev)[
            args.root_partition_number
        ].start
        start_bytes = start_sector * SECTOR_BYTES

        with LoopDev(args, args.image, offset=start_bytes) as root_dev:
            print("Resizing ext filesystem...")
            new_size_bytes = resize2fs(args, root_dev)
            print("Resized to %.1f GB" % (new_size_bytes / (2**30)))

            print("Zeroing free blocks...")
            try:
                subprocess.check_call(["sudo", "zerofree", root_dev])
            except BaseException:
                print(
                    "WARNING: Failed to zero free blocks, compressed image will be larger.",
                    file=sys.stderr,
                )
                print("WARNING: Have you installed zerofree?", file=sys.stderr)

        print("Updating partition table...")
        new_end_sector = start_sector + new_size_bytes // SECTOR_BYTES - 1
        resize_part(
            args, disk_dev, args.root_partition_number, start_sector, new_end_sector
        )

        written_end = get_partition_info(args, disk_dev)[args.root_partition_number].end
        if written_end != new_end_sector:
            print("End sector not written correctly:", file=sys.stderr)
            print(
                "  expected %d, read %d" % (new_end_sector, written_end),
                file=sys.stderr,
            )
            sys.exit(1)

    print("Truncating image..")
    with open(args.image, "ab") as f:
        total_size_bytes = start_sector * SECTOR_BYTES + new_size_bytes
        f.truncate(total_size_bytes)


def set_expand_boot(args, boot_mnt, expand):
    """Add/remove flags to kernel command line to resize partition on next boot."""

    cmdline_path = os.path.join(boot_mnt, "cmdline.txt")
    cmdline = open(cmdline_path).read()
    CMDLINE_EXPAND = " quiet init=/usr/lib/raspi-config/init_resize.sh"

    if not expand:
        new_cmdline = cmdline.replace(CMDLINE_EXPAND, "")
    else:
        if CMDLINE_EXPAND not in cmdline:
            new_cmdline = cmdline.strip() + CMDLINE_EXPAND + "\n"
        else:
            new_cmdline = cmdline

    if cmdline != new_cmdline:
        log(args, "previous cmdline:", repr(cmdline))
        log(args, "new cmdline:     ", repr(new_cmdline))
        open(cmdline_path, "w").write(new_cmdline)


def set_expand_root(args, root_mnt, expand):
    """Delete or create /etc/init.d/resize2fs_once."""

    SCRIPT_NAME = "resize2fs_once"
    SCRIPT_RELATIVE_PATH = os.path.join("..", "init.d", SCRIPT_NAME)
    LINK_NAME = "S01" + SCRIPT_NAME
    RUNLEVELS = "3"
    SCRIPT = """#!/bin/sh
### BEGIN INIT INFO
# Provides:          resize2fs_once
# Required-Start:
# Required-Stop:
# Default-Start: 3
# Default-Stop:
# Short-Description: Resize the root filesystem to fill partition
# Description:
### END INIT INFO
. /lib/lsb/init-functions
case "$1" in
  start)
    log_daemon_msg "Starting resize2fs_once"
    ROOT_DEV=`grep -Eo 'root=[[:graph:]]+' /proc/cmdline | cut -d '=' -f 2-` &&
    resize2fs $ROOT_DEV &&
    update-rc.d resize2fs_once remove &&
    rm /etc/init.d/resize2fs_once &&
    log_end_msg $?
    ;;
  *)
    echo "Usage: $0 start" >&2
    exit 3
    ;;
esac
"""

    script_path = os.path.join(root_mnt, "etc", "init.d", SCRIPT_NAME)
    script_links = [
        os.path.join(root_mnt, "etc", "rc%s.d" % runlevel, LINK_NAME)
        for runlevel in RUNLEVELS.split()
    ]
    link_glob = os.path.join(root_mnt, "etc", "rc?.d", "???" + SCRIPT_NAME)

    if expand and not os.path.exists(script_path):
        with open(script_path, "w") as f:
            log(args, "Writing %s..." % script_path)
            f.write(SCRIPT)
        os.chmod(script_path, stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

        for link in script_links:
            log(args, "Creating %s..." % link)
            os.symlink(SCRIPT_RELATIVE_PATH, link)

    if not expand:
        if os.path.exists(script_path):
            log(args, "Deleting %s..." % script_path)
            os.remove(script_path)

        for link in glob.glob(link_glob):
            log(args, "Deleting %s..." % link)
            os.remove(link)


def set_expand(args, expand):
    print("Creating loopback device...")
    with LoopDev(args, args.image) as disk_dev:
        print("Reading partition info...")
        paritition_info = get_partition_info(args, disk_dev)
        boot_start_bytes = (
            paritition_info[args.boot_partition_number].start * SECTOR_BYTES
        )
        root_start_bytes = (
            paritition_info[args.root_partition_number].start * SECTOR_BYTES
        )

        with Mount(args, args.image, offset=boot_start_bytes) as boot_mnt, Mount(
            args, args.image, offset=root_start_bytes
        ) as root_mnt:
            try:
                set_expand_boot(args, boot_mnt, expand)
                set_expand_root(args, root_mnt, expand)
            except PermissionError:
                print(
                    "ERROR: Script must be run with sudo to set [no-]expand",
                    file=sys.stderr,
                )
                sys.exit(1)


def do_compress(args):
    print("Compressing image...")
    subprocess.check_call(["xz", "-%d" % args.compression_level, "--force", args.image])


def main():
    parser = argparse.ArgumentParser(
        description="Reduce the size of a Raspberry Pi SD card image"
    )

    parser.add_argument("image", help="path of SD card image")
    parser.add_argument(
        "--boot-partition-number",
        type=int,
        default=1,
        help="boot partition number (default: 1)",
    )
    parser.add_argument(
        "--root-partition-number",
        type=int,
        default=2,
        help="root partition number (default: 2)",
    )

    parser.add_argument(
        "--no-resize",
        action="store_false",
        dest="resize",
        help="Don't resize the root partition",
    )
    parser.add_argument(
        "--no-expand",
        action="store_false",
        dest="expand",
        help="Disable automatic partition expansion on next boot",
    )
    parser.add_argument(
        "--no-compress",
        action="store_false",
        dest="compress",
        help="Don't compress the final image",
    )
    parser.add_argument(
        "--compression-level",
        type=int,
        default=3,
        help="Compression level at which to run xz",
    )

    parser.add_argument(
        "--verbose", action="store_true", help="show the output of the child processes"
    )

    args = parser.parse_args()

    if args.verbose:
        # inherit from parent
        args.stdout, args.stderr = None, None
    else:
        args.stdout, args.stderr = subprocess.DEVNULL, subprocess.DEVNULL

    if args.resize:
        do_resize(args)

    set_expand(args, args.expand)

    if args.compress:
        do_compress(args)

    print("Completed successfully!")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as ex:
        import traceback

        traceback.print_exc()
        if ex.output:
            sys.stderr.buffer.write(ex.output)
        sys.exit(1)
