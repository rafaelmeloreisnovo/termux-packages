#!/usr/bin/env python3
"""Validate exact-source custody for the RAFCODEPHI freestanding module package."""

import hashlib
import json
import re
from pathlib import Path

MANIFEST = Path("docs/assurance/rafcodephi-freestanding-modules.v1.json")
SHA40 = re.compile(r"^[0-9a-f]{40}$")


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def read(path: str) -> str:
    p = Path(path)
    if not p.is_file():
        fail(f"missing file: {path}")
    return p.read_text(encoding="utf-8")


def git_blob_sha(path: str) -> str:
    data = Path(path).read_bytes()
    header = f"blob {len(data)}\0".encode("ascii")
    return hashlib.sha1(header + data).hexdigest()


if not MANIFEST.is_file():
    fail("manifest missing")
m = json.loads(MANIFEST.read_text(encoding="utf-8"))
if m.get("schema") != "rafcodephi.freestanding-modules/v1":
    fail("schema")
if m.get("claim_allowed") is not False:
    fail("claim_allowed")
mod = m.get("module", {})
if mod.get("package") != "rafcodephi-rmr-vector-field":
    fail("package identity")
if mod.get("origin_repository") != "rafaelmeloreisnovo/Vectras-VM-Android":
    fail("origin repository")
if not SHA40.fullmatch(mod.get("origin_commit", "")):
    fail("origin commit")
if mod.get("origin_commit") != "34aa3db434627c40cdc6fdb595591634e74d090d":
    fail("unexpected source pin")
if mod.get("parent_project_original_author") != "xoureldeen":
    fail("Vectras original author custody")
if mod.get("license_expression") != "GPL-2.0-only":
    fail("module license")
if mod.get("copy_mode") != "BYTE_IDENTICAL_SOURCE_TRANSCRIPTION":
    fail("copy mode")

expected = {
    "packages/rafcodephi-rmr-vector-field/files/rmr_vector_field.c": "8dfb230410b20e36236dbc3863d301e16a4c595d",
    "packages/rafcodephi-rmr-vector-field/files/rmr_vector_field.h": "10cb0ab97fcb59ca691f8b216ca9165231e6de74",
    "packages/rafcodephi-rmr-vector-field/files/rmr_types.h": "433843be4b8a1c1a722bc814b63c366db3314cd6",
}
manifest_files = {entry["package_path"]: entry["git_blob"] for entry in mod.get("source_files", [])}
if manifest_files != expected:
    fail("manifest file/blob map")
for path, expected_sha in expected.items():
    if not Path(path).is_file():
        fail(f"copied source missing: {path}")
    actual = git_blob_sha(path)
    if actual != expected_sha:
        fail(f"byte identity lost: {path}: {actual} != {expected_sha}")
    text = read(path)
    if "SPDX-License-Identifier: GPL-2.0-only" not in text:
        fail(f"SPDX lost: {path}")
    if "Copyright (C) Rafael M. R." not in text:
        fail(f"source copyright lost: {path}")

build = read("packages/rafcodephi-rmr-vector-field/build.sh")
for token in (
    'TERMUX_PKG_LICENSE="GPL-2.0-only"',
    "Origin-Repository: rafaelmeloreisnovo/Vectras-VM-Android",
    "Origin-Commit: 34aa3db434627c40cdc6fdb595591634e74d090d",
    "Governing-License: GPL-2.0-only",
    "-ffreestanding",
    "-fno-builtin",
    "-fno-stack-protector",
    '"$NM" -u',
):
    if token not in build:
        fail(f"build custody/compile contract missing: {token}")
if "TERMUX_PKG_SRCURL" in build:
    fail("byte-identical local transcription must not silently fetch a different source")

# Current Termux buildorder creates a virtual <parent>-static subpackage for
# every main package. An explicit file with the same name is a duplicate package
# definition and must not exist. The archive remains installed under lib/*.a and
# is split by the native static-package mechanism.
explicit_static = Path("packages/rafcodephi-rmr-vector-field/rafcodephi-rmr-vector-field-static.subpackage.sh")
if explicit_static.exists():
    fail("explicit -static subpackage duplicates Termux virtual static split")
buildorder = read("scripts/buildorder.py")
if "self.name + '-static'" not in buildorder or "virtual=True" not in buildorder:
    fail("Termux virtual static split contract not found")
if "lib/librafcodephi-rmr-vector-field.a" not in build:
    fail("static archive install path missing")

origin = read("packages/rafcodephi-rmr-vector-field/ORIGIN.md")
for token in (
    "rafaelmeloreisnovo/Vectras-VM-Android",
    "34aa3db434627c40cdc6fdb595591634e74d090d",
    "xoureldeen",
    "GPL-2.0-only",
):
    if token not in origin:
        fail(f"ORIGIN missing: {token}")

contract = mod.get("compile_contract", {})
if contract.get("heap") is not False or contract.get("libc") is not False or contract.get("libm") is not False:
    fail("freestanding dependency contract")
if contract.get("unresolved_external_symbols_allowed") is not False:
    fail("undefined symbol contract")
if contract.get("targets") != ["armv7a-linux-androideabi21", "aarch64-linux-android21"]:
    fail("target contract")

runtime_recipe = read("packages/rafcodephi-runtime-profile/build.sh")
promotion = mod.get("profile_promotion", {}).get("current")
if promotion == "CANDIDATE_PACKAGE_NOT_IN_DEFAULT_PROFILE":
    if "rafcodephi-rmr-vector-field" in runtime_recipe:
        fail("candidate package promoted before evidence")
elif promotion == "PROMOTED_RUNTIME_PROVEN_STRUCTURAL":
    if "rafcodephi-rmr-vector-field" not in runtime_recipe:
        fail("promoted package missing from runtime profile")
else:
    fail("unknown promotion state")

print("PASS: RMR vector-field preserves byte-identical GPLv2 custody, native virtual static split and freestanding gates")
