#!/usr/bin/env python3
"""
Manifest generator: Converts build.sh files to binary manifest format.
This tool performs offline conversion, so no parsing overhead at build time.
"""

import os
import re
import sys
import struct
from collections import defaultdict, deque

TERMUX_MANIFEST_MAGIC = 0x5445524D  # "TERM"
TERMUX_MANIFEST_VERSION = 1
TERMUX_PKG_NAME_LEN = 64
TERMUX_PKG_VERSION_LEN = 32
TERMUX_MAX_DEPS = 16

ARCH_MAP = {
    "aarch64": 0,
    "arm": 1,
    "x86_64": 2,
    "i686": 3,
}

class Package:
    def __init__(self, name):
        self.name = name
        self.version = ""
        self.arch = 0
        self.api_level = 24
        self.flags = 0
        self.sha256 = b'\x00' * 32
        self.deps = []
        self.source_url = ""
        self.patches = ""
        self.configure_args = ""

    def to_bytes(self, string_pool_offset_fn):
        dep_ids = list(range(len(self.deps))) + [0] * (TERMUX_MAX_DEPS - len(self.deps))
        entry = struct.pack(
            '<64s32sBBH8IHH4I16H',
            self.name.encode().ljust(TERMUX_PKG_NAME_LEN, b'\x00'),
            self.version.encode().ljust(TERMUX_PKG_VERSION_LEN, b'\x00'),
            self.arch,
            self.api_level,
            self.flags,
            *struct.unpack('<8I', self.sha256),
            len(self.deps),
            0,
            string_pool_offset_fn(self.source_url),
            string_pool_offset_fn(self.patches),
            string_pool_offset_fn(self.configure_args),
            0,
            *dep_ids
        )
        return entry

def parse_buildsh(build_sh_path):
    """Parse a build.sh file and extract metadata."""
    try:
        with open(build_sh_path, 'r') as f:
            content = f.read()
    except Exception as e:
        print(f"Warning: Could not read {build_sh_path}: {e}", file=sys.stderr)
        return None

    pkg = Package(os.path.basename(os.path.dirname(build_sh_path)))

    # Extract metadata using regex
    name_match = re.search(r'TERMUX_PKG_NAME=(["\']?)([^"\']+)\1', content)
    if name_match:
        pkg.name = name_match.group(2)

    version_match = re.search(r'TERMUX_PKG_VERSION=(["\']?)([^"\']+)\1', content)
    if version_match:
        pkg.version = version_match.group(2)

    source_match = re.search(r'TERMUX_PKG_SRCURL=(["\']?)([^"\']+)\1', content)
    if source_match:
        pkg.source_url = source_match.group(2)

    # Extract dependencies
    deps_match = re.search(r'TERMUX_PKG_DEPENDS=(["\']?)([^"\']+)\1', content)
    if deps_match:
        deps_str = deps_match.group(2)
        pkg.deps = [d.strip() for d in deps_str.split(',')][:TERMUX_MAX_DEPS]

    # Extract SHA256
    sha_match = re.search(r'TERMUX_PKG_SHA256=(["\']?)([a-f0-9]+)\1', content)
    if sha_match:
        sha_hex = sha_match.group(2)
        if len(sha_hex) == 64:
            pkg.sha256 = bytes.fromhex(sha_hex)

    # Extract configure args
    conf_match = re.search(r'TERMUX_PKG_EXTRA_CONFIGURE_ARGS=(["\']?)([^"\']+)\1', content)
    if conf_match:
        pkg.configure_args = conf_match.group(2)

    # Check flags
    if 'TERMUX_PKG_KEEP_STATIC_LIBS' in content:
        pkg.flags |= 0x0001
    if 'TERMUX_PKG_NO_STATICALLY_LINKED_EXECUTABLES' in content:
        pkg.flags |= 0x0002
    if 'TERMUX_PKG_CLANG_ONLY' in content:
        pkg.flags |= 0x0004

    return pkg

def topological_sort(packages):
    """Perform topological sort on packages by dependencies."""
    pkg_map = {pkg.name: pkg for pkg in packages}
    in_degree = {pkg.name: 0 for pkg in packages}
    graph = defaultdict(list)

    # Build dependency graph
    for pkg in packages:
        for dep in pkg.deps:
            if dep in pkg_map:
                graph[dep].append(pkg.name)
                in_degree[pkg.name] += 1

    # Kahn's algorithm
    queue = deque([name for name in in_degree if in_degree[name] == 0])
    sorted_names = []

    while queue:
        name = queue.popleft()
        sorted_names.append(name)

        for neighbor in graph[name]:
            in_degree[neighbor] -= 1
            if in_degree[neighbor] == 0:
                queue.append(neighbor)

    if len(sorted_names) != len(packages):
        print("Warning: Circular dependency detected", file=sys.stderr)
        return packages

    return [pkg_map[name] for name in sorted_names if name in pkg_map]

def generate_manifest(packages_dir, output_path):
    """Generate binary manifest from package directory."""
    packages = []

    # Scan all packages
    if not os.path.isdir(packages_dir):
        print(f"Error: {packages_dir} is not a directory", file=sys.stderr)
        return 1

    for pkg_name in sorted(os.listdir(packages_dir)):
        pkg_path = os.path.join(packages_dir, pkg_name)
        build_sh = os.path.join(pkg_path, "build.sh")

        if not os.path.isfile(build_sh):
            continue

        pkg = parse_buildsh(build_sh)
        if pkg:
            packages.append(pkg)

    print(f"Loaded {len(packages)} packages")

    # Topologically sort
    packages = topological_sort(packages)
    print(f"Sorted {len(packages)} packages by dependency")

    # Build string pool
    string_pool = b""
    string_offsets = {}

    def add_string(s):
        nonlocal string_pool
        if not s:
            return 0
        if s in string_offsets:
            return string_offsets[s]
        offset = len(string_pool)
        string_offsets[s] = offset
        string_pool += s.encode() + b'\x00'
        return offset

    # Pre-populate string pool
    for pkg in packages:
        add_string(pkg.source_url)
        add_string(pkg.patches)
        add_string(pkg.configure_args)

    # Calculate structure sizes (must match C struct)
    # struct termux_pkg_manifest: 64 + 32 + 1 + 1 + 2 + 32 + 2 + 2 + 16 + 32 = 184 bytes
    ENTRY_SIZE = 184
    HEADER_SIZE = 20  # 5 * uint32_t

    # Write manifest
    try:
        with open(output_path, 'wb') as f:
            # Write header
            string_pool_offset = HEADER_SIZE + HEADER_SIZE + len(packages) * ENTRY_SIZE
            header = struct.pack(
                '<IIIII',
                TERMUX_MANIFEST_MAGIC,
                TERMUX_MANIFEST_VERSION,
                len(packages),
                0,  # num_deps
                string_pool_offset,
            )
            f.write(header)
            f.write(struct.pack('<I', len(string_pool)))  # string_pool_size

            # Write package entries
            for i, pkg in enumerate(packages):
                try:
                    entry_data = pkg.to_bytes(lambda s: string_offsets.get(s, 0))
                    f.write(entry_data)
                except Exception as e:
                    print(f"Error at package {i}: {pkg.name}", file=sys.stderr)
                    print(f"  deps: {pkg.deps}", file=sys.stderr)
                    print(f"  error: {e}", file=sys.stderr)
                    raise

            # Write string pool
            f.write(string_pool)
            total_size = f.tell()

        print(f"✓ Manifest written to {output_path}")
        print(f"  Packages: {len(packages)}")
        print(f"  String pool: {len(string_pool)} bytes")
        print(f"  Total size: {total_size} bytes")
        return 0

    except Exception as e:
        print(f"Error writing manifest: {e}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: manifest_generator.py <packages-dir> [output-path]")
        print("  packages-dir: Path to packages/ directory")
        print("  output-path: Output manifest.bin (default: ./manifest.bin)")
        sys.exit(1)

    packages_dir = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else "manifest.bin"

    sys.exit(generate_manifest(packages_dir, output_path))
