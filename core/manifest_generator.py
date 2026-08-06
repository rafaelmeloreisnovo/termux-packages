#!/usr/bin/env python3
"""Generate the v1 binary manifest from package build.sh metadata.

This remains an offline converter. It is deliberately fail-closed: values that
cannot fit the v1 contract are reported instead of silently truncated.
"""

from __future__ import annotations

import os
import re
import struct
import sys
from collections import defaultdict, deque
from pathlib import Path
from typing import Callable

TERMUX_MANIFEST_MAGIC = 0x5445524D  # "TERM"
TERMUX_MANIFEST_VERSION = 1
TERMUX_PKG_NAME_LEN = 64
TERMUX_PKG_VERSION_LEN = 32
TERMUX_MAX_DEPS = 16
ENTRY_SIZE = 184
HEADER_SIZE = 20
SIZE_FIELD_SIZE = 4
UNRESOLVED_DEP_ID = 0xFFFF


class ManifestError(RuntimeError):
    pass


class Package:
    def __init__(self, name: str) -> None:
        self.name = name
        self.version = ""
        self.arch = 0
        self.api_level = 24
        self.flags = 0
        self.sha256 = b"\x00" * 32
        self.deps: list[str] = []
        self.source_url = ""
        self.patches = ""
        self.configure_args = ""

    def to_bytes(
        self,
        string_pool_offset_fn: Callable[[str], int],
        package_ids: dict[str, int],
    ) -> bytes:
        if not self.name or len(self.name.encode()) >= TERMUX_PKG_NAME_LEN:
            raise ManifestError(f"invalid package name length: {self.name!r}")
        if not self.version or len(self.version.encode()) >= TERMUX_PKG_VERSION_LEN:
            raise ManifestError(f"invalid/empty version for {self.name}: {self.version!r}")
        if len(self.deps) > TERMUX_MAX_DEPS:
            raise ManifestError(
                f"{self.name} has {len(self.deps)} dependencies; v1 limit is {TERMUX_MAX_DEPS}"
            )

        dep_ids = [package_ids.get(dep, UNRESOLVED_DEP_ID) for dep in self.deps]
        dep_ids.extend([0] * (TERMUX_MAX_DEPS - len(dep_ids)))
        entry = struct.pack(
            "<64s32sBBH8IHH4I16H",
            self.name.encode().ljust(TERMUX_PKG_NAME_LEN, b"\x00"),
            self.version.encode().ljust(TERMUX_PKG_VERSION_LEN, b"\x00"),
            self.arch,
            self.api_level,
            self.flags,
            *struct.unpack("<8I", self.sha256),
            len(self.deps),
            0,
            string_pool_offset_fn(self.source_url),
            string_pool_offset_fn(self.patches),
            string_pool_offset_fn(self.configure_args),
            0,
            *dep_ids,
        )
        if len(entry) != ENTRY_SIZE:
            raise ManifestError(
                f"entry size drift for {self.name}: {len(entry)} != {ENTRY_SIZE}"
            )
        return entry


def _extract(content: str, variable: str) -> str | None:
    match = re.search(rf"{re.escape(variable)}=([\"']?)([^\"'\n]+)\1", content)
    return match.group(2).strip() if match else None


def parse_buildsh(build_sh_path: Path) -> Package | None:
    try:
        content = build_sh_path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"Warning: could not read {build_sh_path}: {exc}", file=sys.stderr)
        return None

    pkg = Package(build_sh_path.parent.name)
    pkg.name = _extract(content, "TERMUX_PKG_NAME") or pkg.name
    pkg.version = _extract(content, "TERMUX_PKG_VERSION") or ""
    pkg.source_url = _extract(content, "TERMUX_PKG_SRCURL") or ""
    pkg.configure_args = _extract(content, "TERMUX_PKG_EXTRA_CONFIGURE_ARGS") or ""

    deps_raw = _extract(content, "TERMUX_PKG_DEPENDS") or ""
    pkg.deps = [part.strip() for part in deps_raw.split(",") if part.strip()]

    sha_hex = _extract(content, "TERMUX_PKG_SHA256") or ""
    if sha_hex:
        if not re.fullmatch(r"[0-9a-fA-F]{64}", sha_hex):
            raise ManifestError(f"invalid SHA-256 for {pkg.name}: {sha_hex!r}")
        pkg.sha256 = bytes.fromhex(sha_hex)

    if "TERMUX_PKG_KEEP_STATIC_LIBS" in content:
        pkg.flags |= 0x0001
    if "TERMUX_PKG_NO_STATICALLY_LINKED_EXECUTABLES" in content:
        pkg.flags |= 0x0002
    if "TERMUX_PKG_CLANG_ONLY" in content:
        pkg.flags |= 0x0004

    return pkg


def topological_sort(packages: list[Package]) -> list[Package]:
    pkg_map = {pkg.name: pkg for pkg in packages}
    in_degree = {pkg.name: 0 for pkg in packages}
    graph: dict[str, list[str]] = defaultdict(list)

    for pkg in packages:
        for dep in pkg.deps:
            if dep in pkg_map:
                graph[dep].append(pkg.name)
                in_degree[pkg.name] += 1

    queue = deque(sorted(name for name, degree in in_degree.items() if degree == 0))
    sorted_names: list[str] = []
    while queue:
        name = queue.popleft()
        sorted_names.append(name)
        for neighbor in sorted(graph[name]):
            in_degree[neighbor] -= 1
            if in_degree[neighbor] == 0:
                queue.append(neighbor)

    if len(sorted_names) != len(packages):
        cyclic = sorted(name for name, degree in in_degree.items() if degree > 0)
        raise ManifestError(f"circular dependency detected: {', '.join(cyclic[:20])}")
    return [pkg_map[name] for name in sorted_names]


def generate_manifest(packages_dir: Path, output_path: Path) -> int:
    if not packages_dir.is_dir():
        raise ManifestError(f"not a package directory: {packages_dir}")

    packages: list[Package] = []
    for pkg_path in sorted(packages_dir.iterdir()):
        build_sh = pkg_path / "build.sh"
        if not build_sh.is_file():
            continue
        pkg = parse_buildsh(build_sh)
        if pkg:
            packages.append(pkg)

    if not packages:
        raise ManifestError("no build.sh package definitions found")

    packages = topological_sort(packages)
    package_ids = {pkg.name: index for index, pkg in enumerate(packages)}

    # Offset 0 is reserved as the unambiguous empty-string sentinel.
    string_pool = bytearray(b"\x00")
    string_offsets: dict[str, int] = {}

    def add_string(value: str) -> int:
        if not value:
            return 0
        existing = string_offsets.get(value)
        if existing is not None:
            return existing
        offset = len(string_pool)
        string_offsets[value] = offset
        string_pool.extend(value.encode("utf-8"))
        string_pool.append(0)
        return offset

    for pkg in packages:
        add_string(pkg.source_url)
        add_string(pkg.patches)
        add_string(pkg.configure_args)

    entries = [pkg.to_bytes(add_string, package_ids) for pkg in packages]
    string_pool_offset = HEADER_SIZE + SIZE_FIELD_SIZE + len(entries) * ENTRY_SIZE
    header = struct.pack(
        "<IIIII",
        TERMUX_MANIFEST_MAGIC,
        TERMUX_MANIFEST_VERSION,
        len(entries),
        0,
        string_pool_offset,
    )

    unresolved = sorted(
        {dep for pkg in packages for dep in pkg.deps if dep not in package_ids}
    )
    temp_path = output_path.with_suffix(output_path.suffix + ".tmp")
    with temp_path.open("wb") as stream:
        stream.write(header)
        stream.write(struct.pack("<I", len(string_pool)))
        for entry in entries:
            stream.write(entry)
        stream.write(string_pool)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temp_path, output_path)

    print(f"Manifest written: {output_path}")
    print(f"packages={len(entries)} entry_size={ENTRY_SIZE}")
    print(f"string_pool_offset={string_pool_offset} string_pool_size={len(string_pool)}")
    print(f"unresolved_dependency_names={len(unresolved)}")
    if unresolved:
        print(
            "TOKEN_VAZIO_UNRESOLVED_DEPENDENCIES=" + ",".join(unresolved[:32]),
            file=sys.stderr,
        )
    return 0


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("Usage: manifest_generator.py <packages-dir> [output-path]", file=sys.stderr)
        return 2
    packages_dir = Path(argv[1])
    output_path = Path(argv[2]) if len(argv) > 2 else Path("manifest.bin")
    try:
        return generate_manifest(packages_dir, output_path)
    except ManifestError as exc:
        print(f"MANIFEST_GENERATION_FAILED: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
