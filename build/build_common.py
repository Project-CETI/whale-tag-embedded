"""
Common utilities for building image.
"""

import contextlib
import distutils.spawn
import os
import shutil
import subprocess
import sys

from imgtools import (
    SECTOR_BYTES,
    BindMount,
    LoopDev,
    Mount,
    MountAt,
    get_partition_info,
    resize2fs,
    resize_part,
    create_part,
    mkfs,
)


def do_expand(args, image_file, expand_bytes=(2**30)):
    """Expand the root filesystem on the image."""

    expand_sectors = expand_bytes // SECTOR_BYTES

    print("Extending the image file by %d bytes to enlarge root partition" % expand_bytes)
    with open(image_file, "ab") as f:
        f.truncate(f.tell() + expand_bytes)

    with LoopDev(args, image_file) as disk_dev:
        partition_info = get_partition_info(args, disk_dev)
        print(
            "Increasing the size of the root partition by %d secrors" % expand_sectors
        )
        root_partition = partition_info[args.root_partition_number]
        start_sector = root_partition.start
        end_sector = root_partition.end + expand_sectors
        resize_part(
            args, disk_dev, args.root_partition_number, start_sector, end_sector
        )


    print("Increasing the size of the root filesystem...")
    start_bytes = start_sector * SECTOR_BYTES
    with LoopDev(args, image_file, offset=start_bytes) as root_dev:
        new_size_bytes = resize2fs(args, root_dev, "maximum")
        print("Resized to %.1f GB" % (new_size_bytes / (2 ** 30)))
        subprocess.check_call(
            ["sudo", "zerofree", root_dev], stdout=args.stdout, stderr=args.stderr
        )
        print("Zeroed free blocks in root partition")

def do_add_data_partition(args, image_file, expand_bytes=(2**30)):
    """Create an ext4 partition at the end of the device with label cetiData."""

    expand_sectors = expand_bytes // SECTOR_BYTES

    print("Extending the image file by %d bytes for data partition" % expand_bytes)
    with open(image_file, "ab") as f:
        f.truncate(f.tell() + expand_bytes)

    with LoopDev(args, image_file) as disk_dev:
        partition_info = get_partition_info(args, disk_dev)
        root_partition = partition_info[args.root_partition_number]
        print("Creating the data partition")
        partition_number = args.root_partition_number + 1
        start_sector = root_partition.end + 1
        end_sector = start_sector + expand_sectors - 1
        create_part(args, disk_dev, partition_number, start_sector, end_sector)


    print("Creating filesystem with label cetiData...")
    start_bytes = start_sector * SECTOR_BYTES
    size_bytes = (end_sector - start_sector) * SECTOR_BYTES
    with LoopDev(args, image_file, offset=start_bytes) as data_dev:
        mkfs(args, data_dev)
        subprocess.check_call(
            ["sudo", "zerofree", data_dev], stdout=args.stdout, stderr=args.stderr
        )
        print("Zeroed free blocks in data partition")



@contextlib.contextmanager
def SetupEmulator(args, root_mnt):
    """Set up the QEMU emulator so it works from within the chroot.

    As described here: https://gist.github.com/cleverca22/1f2c2f60bf37de68cf0d7917cbbef9c7
    """

    qemu_src_path = distutils.spawn.find_executable("qemu-system-arm")
    qemu_dst_path = os.path.join(root_mnt, "usr", "bin", "qemu-system-arm")

    if qemu_src_path is None:
        print("Failed to find qemu-system-arm.", file=sys.stderr)
        print(
            "    sudo apt-get install qemu qemu-system-arm binfmt-support",
            file=sys.stderr,
        )
        sys.exit(1)


    try:
        shutil.copy(qemu_src_path, qemu_dst_path)

        try:
            yield
        finally:
            os.unlink(qemu_dst_path)

    finally:
        pass


@contextlib.contextmanager
def MountPoint(path):
    def find_existing(path):
        name = None
        while path:
            if os.path.exists(path):
                return (path, name)
            path, name = os.path.split(path)

    existing_path, name = find_existing(path)
    if name:
        os.makedirs(path)
        try:
            yield
        finally:
            shutil.rmtree(os.path.join(existing_path, name))
    else:
        yield


@contextlib.contextmanager
def MountAndSetupEmulator(args, partition_info, image_file, mounts=None):
    boot_start_bytes = partition_info[args.boot_partition_number].start * SECTOR_BYTES
    root_start_bytes = partition_info[args.root_partition_number].start * SECTOR_BYTES

    all_mounts = [
        ("/dev", "/dev"),
        ("/sys", "/sys"),
        ("/proc", "/proc"),
        ("/dev/pts", "/dev/pts"),
    ] + (mounts or [])

    with Mount(args, image_file, offset=root_start_bytes) as root_mnt:
        with contextlib.ExitStack() as stack:
            stack.enter_context(
                MountAt(
                    args,
                    image_file,
                    os.path.join(root_mnt, "boot"),
                    offset=boot_start_bytes,
                )
            )
            for host_path, img_path in all_mounts:
                assert os.path.exists(host_path)
                assert os.path.isabs(img_path)
                abs_img_path = os.path.join(root_mnt, img_path[1:])
                print("Bind mount: %s => %s" % (host_path, abs_img_path))
                stack.enter_context(MountPoint(abs_img_path))
                stack.enter_context(BindMount(args, host_path, abs_img_path))
            stack.enter_context(SetupEmulator(args, root_mnt))
            yield root_mnt


def run_in_chroot(args, root_mnt, cmd, env=None):
    """Run the given command chrooted into the image."""
    if not cmd:
        raise ValueError("cmd must not be empty")
    elif cmd[0].startswith("/") or not os.path.isfile(os.path.join(root_mnt, cmd[0])):
        raise ValueError("cmd[0] must be a relative path to a binary in the image")

    env_list = ["%s=%s" % item for item in (env or {}).items()] + [
        "LANG=C.UTF-8",  # en_US isn't installed on the default RPi image, so use C
        "LANGUAGE=C:",
        "LC_CTYPE=C.UTF-8",
        "QEMU_CPU=cortex-a53",  # Use armv6l for compatability with Pi Zero.
    ]

    chroot_cmd = ["sudo"] + env_list + ["chroot", "."] + cmd
    return subprocess.call(
        chroot_cmd, cwd=root_mnt, stdout=args.stdout, stderr=args.stderr
    )
