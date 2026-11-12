#!/usr/bin/env python3
"""Verify license documents in an installed SDK or a ZIP/TGZ/DEB package."""

import io
import posixpath
import re
from urllib.parse import unquote, urlsplit
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tarfile
import zipfile


def read_files(path):
    if path.is_dir():
        return {p.relative_to(path).as_posix(): p.read_bytes()
                for p in path.rglob("*") if p.is_file()}
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            return {n: archive.read(n) for n in archive.namelist()
                    if not n.endswith("/")}
    if path.suffix == ".deb":
        data = subprocess.check_output(["dpkg-deb", "--fsys-tarfile", str(path)])
        archive = tarfile.open(fileobj=io.BytesIO(data))
    else:
        archive = tarfile.open(path)
    with archive:
        return {m.name: archive.extractfile(m).read()
                for m in archive.getmembers() if m.isfile()}


def verify(path):
    raw_files = read_files(path)
    files = {posixpath.normpath(name): data for name, data in raw_files.items()}
    if len(files) != len(raw_files):
        raise ValueError("duplicate normalized paths in distribution")
    repo = Path(__file__).resolve().parent.parent
    docs = ("LICENSE.md", "NOTICE.md", "THIRD_PARTY_NOTICES.md")
    for name in files:
        parts = PurePosixPath(name).parts
        if "legal" in parts or parts[-1].startswith(("TermOfUse", "PrivacyPolicy")):
            raise ValueError(f"obsolete legal document: {name}")
        if ("DivingStationProtocolSpecs" in parts or "protocol" in parts
                or parts[-1] in ("ds_protocol_v1.md", "ds_protocol_v2.md")):
            raise ValueError(f"unexpected bundled protocol specification: {name}")
    for doc in docs:
        expected = (repo / doc).read_text(encoding="utf-8")
        matches = [data for name, data in files.items()
                   if name.endswith(f"share/doc/ContactGloveSDKv2/{doc}")]
        if len(matches) != 1 or matches[0].decode("utf-8").replace("\r\n", "\n") != expected:
            raise ValueError(f"missing or changed distribution document: {doc}")
    # Source links are absolute for repository rendering; distributions use
    # relative links and omit the maintainer-only guidance.
    base = "https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/"
    for locale, filename, other, contact in (
        ("en", "README.md", "README_JA.md", "https://melt-interface-technologies.com/en/contact/"),
        ("ja", "README_JA.md", "README.md", "https://melt-interface-technologies.com/contact/"),
    ):
        source = (repo / filename).read_text(encoding="utf-8")
        url = (f"https://docs.melt-interface-technologies.com/{locale}/docs/contact/"
               "sdks/contact-glove-v2-native")
        for required in (base + other, url, url + "/api", contact, "C++17", "x86_64",
                         "macOS 13", "contactglove-basic-usage", "sudo apt-get update",
                         "sudo apt-get install contactglove-sdk-v2",
                         f"https://docs.melt-interface-technologies.com/{locale}/docs/contact/sdks/apt-repository",
                         url + ("#install-from-apt" if locale == "en" else "#aptからインストールする"),
                         "https://apt.melt-interface-technologies.com/ubuntu"):
            if required not in source:
                raise ValueError(f"missing setup requirement in {filename}: {required}")
        source_without_code = re.sub(r"```[^\n]*\n.*?```", "", source, flags=re.S)
        source_links = re.findall(r"(?<!!)\[[^\]]+\]\(([^)]+)\)", source_without_code)
        if any(not target.startswith(("https://", "http://")) for target in source_links):
            raise ValueError(f"source README contains a non-absolute link: {filename}")
        source_for_distribution = re.sub(
            r"<!-- BEGIN SOURCE-ONLY -->.*?<!-- END SOURCE-ONLY -->", "", source, flags=re.S
        )
        def canonical(value):
            return re.sub(r"(?<!!)\[([^\]]+)\]\([^)]+\)", r"[\1](LINK)", value)
        readmes = [(name, data) for name, data in files.items()
                   if name.endswith(f"share/doc/ContactGloveSDKv2/{filename}")]
        if not path.is_dir() and path.suffix != ".deb":
            readmes += [(name, data) for name, data in files.items()
                        if PurePosixPath(name).name == filename
                        and len(PurePosixPath(name).parts) <= 2]
            expected_count = 2
        else:
            expected_count = 1
        if len(readmes) != expected_count:
            raise ValueError(f"missing or duplicate distribution README: {filename}")
        for name, data in readmes:
            text = data.decode("utf-8").replace("\r\n", "\n")
            if base in text or "SOURCE-ONLY" in text or "DEVELOPMENT" in text:
                raise ValueError(f"source-only link in distribution README: {name}")
            if canonical(text) != canonical(source_for_distribution):
                raise ValueError(f"distribution README content differs from source: {name}")
            if contact not in text or url not in text or base + other in text:
                raise ValueError(f"incorrect language/support links in {name}")
            clean = re.sub(r"```[^\n]*\n.*?```", "", text, flags=re.S)
            links = re.findall(r"(?<!!)\[[^\]]+\]\(([^)]+)\)", clean)
            local_targets = []
            for target in links:
                parsed = urlsplit(target)
                if parsed.scheme or target.startswith("#"):
                    continue
                resolved = posixpath.normpath(posixpath.join(posixpath.dirname(name), unquote(parsed.path)))
                if resolved not in files:
                    raise ValueError(f"broken bundled link in {name}: {target}")
                local_targets.append(resolved)
            for suffix in (other, "api.md", "LICENSE.md", "NOTICE.md", "THIRD_PARTY_NOTICES.md",
                           "examples/basic_usage.cpp", "examples/c_basic_usage.c",
                           "rust/README.md", "rust/contactglove-sdk-v2/examples/basic_usage.rs"):
                if not any(target.endswith("/" + suffix) or target == suffix for target in local_targets):
                    raise ValueError(f"missing bundled link in {name}: {suffix}")
    if any(PurePosixPath(name).name == "README_EN.md" for name in files):
        raise ValueError("obsolete README_EN.md found in distribution")
    for crate in ("contactglove-sdk-v2", "contactglove-sdk-v2-sys"):
        manifests = [data for name, data in files.items()
                     if name.endswith(f"rust/{crate}/Cargo.toml")]
        if len(manifests) != 1 or b'license = "Apache-2.0"' not in manifests[0]:
            raise ValueError(f"missing Apache-2.0 metadata: {crate}")
        for doc in docs:
            expected = (repo / doc).read_text(encoding="utf-8")
            matches = [data for name, data in files.items()
                       if name.endswith(f"rust/{crate}/{doc}")]
            if len(matches) != 1 or matches[0].decode("utf-8").replace("\r\n", "\n") != expected:
                raise ValueError(f"missing or changed crate document: {crate}/{doc}")
    print(f"Distribution license verification passed: {path}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(f"usage: {sys.argv[0]} INSTALL_ROOT_OR_PACKAGE [...]")
    for argument in sys.argv[1:]:
        verify(Path(argument))
