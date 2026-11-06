#!/usr/bin/env python3
"""Check repeat installs and preservation of an unknown legacy README."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from verify_distribution_licenses import verify


def install(build, prefix, expect_success):
    result = subprocess.run(
        ["cmake", "--install", str(build), "--config", "Release", "--prefix", str(prefix)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace",
    )
    if (result.returncode == 0) != expect_success:
        raise RuntimeError(f"unexpected install result ({result.returncode}):\n{result.stdout}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="contactglove-readme-reinstall-") as temporary:
        prefix = Path(temporary) / "install"
        docdir = prefix / "share" / "doc" / "ContactGloveSDKv2"
        docdir.mkdir(parents=True)
        legacy = docdir / "README_EN.md"
        sentinel = b"User-edited document: preserve this file.\n"
        legacy.write_bytes(sentinel)
        install(args.build.resolve(), prefix, expect_success=False)
        if legacy.read_bytes() != sentinel:
            raise AssertionError("unknown legacy README was modified")
        legacy.unlink()
        install(args.build.resolve(), prefix, expect_success=True)
        install(args.build.resolve(), prefix, expect_success=True)
        if legacy.exists():
            raise AssertionError("legacy README remains after reinstall")
        verify(prefix)
    print("README reinstall and unknown-file preservation passed")


if __name__ == "__main__":
    main()
