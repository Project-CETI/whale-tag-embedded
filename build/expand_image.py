#!/usr/bin/env python3
"""
To increase file.img by 500MB run:

sudo ./expand_image.py --expand-bytes $((500*1024*1024)) file.img

"""

import argparse
import subprocess

from build_common import do_expand


def main():
    parser = argparse.ArgumentParser(
        description="Expands image file by specified about of bytes",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--boot-partition-number", type=int, default=1, help="boot partition number"
    )
    parser.add_argument(
        "--root-partition-number", type=int, default=2, help="root partition number"
    )
    parser.add_argument(
        "--verbose", action="store_true", help="show the output of the child processes"
    )
    parser.add_argument(
        "--expand-bytes",
        type=int,
        default=(2**30),
        help="expand image file by this amount of bytes",
    )
    parser.add_argument("image", help="path to image file")

    args = parser.parse_args()

    if args.verbose:
        args.stdout, args.stderr = None, None  # inherit from parent
    else:
        args.stdout, args.stderr = subprocess.DEVNULL, subprocess.DEVNULL

    do_expand(args, args.image, args.expand_bytes)


if __name__ == "__main__":
    main()
