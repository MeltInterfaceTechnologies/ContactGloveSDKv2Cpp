#!/usr/bin/env python3
"""Build the bundled Rust example from a writable copy of a read-only workspace."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile


def fingerprint(directory):
    return {p.relative_to(directory).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in directory.rglob("*") if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sdk_root", type=Path)
    args = parser.parse_args()
    sdk_root = args.sdk_root.resolve()
    bundled = sdk_root / "share" / "ContactGloveSDKv2" / "rust"
    with tempfile.TemporaryDirectory(prefix="contactglove-rust-copy-") as temporary:
        root = Path(temporary)
        source = root / "read-only" / "rust"
        workspace = root / "writable" / "rust"
        shutil.copytree(bundled, source)
        # Exercise the case where Cargo must create a lockfile as well as target/.
        (source / "Cargo.lock").unlink(missing_ok=True)
        for path in source.rglob("*"):
            if path.is_file():
                path.chmod(stat.S_IREAD)
        try:
            before = fingerprint(source)
            shutil.copytree(source, workspace)
            for path in workspace.rglob("*"):
                if path.is_file():
                    path.chmod(path.stat().st_mode | stat.S_IWUSR)
            env = os.environ.copy()
            env["CONTACTGLOVE_SDK_ROOT"] = str(sdk_root)
            env.pop("CARGO_TARGET_DIR", None)
            subprocess.run(
                ["cargo", "build", "--offline", "--manifest-path", str(workspace / "Cargo.toml"),
                 "-p", "contactglove-sdk-v2", "--example", "basic_usage"],
                check=True, env=env,
            )
            if not (workspace / "Cargo.lock").is_file() or not (workspace / "target").is_dir():
                raise AssertionError("Cargo did not create the lockfile and output in the writable copy")
            if fingerprint(source) != before:
                raise AssertionError("read-only source workspace was modified")
        finally:
            for path in source.rglob("*"):
                if path.is_file():
                    path.chmod(path.stat().st_mode | stat.S_IWUSR)
    print("Read-only Rust source and writable workspace build passed; example not run")


if __name__ == "__main__":
    main()
