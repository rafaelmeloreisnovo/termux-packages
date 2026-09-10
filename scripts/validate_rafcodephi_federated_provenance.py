#!/usr/bin/env python3
"""Fail-closed RAFCODEPHI federated provenance/profile gate."""

import json
import re
from pathlib import Path

MANIFEST = Path("docs/assurance/rafcodephi-federated-components.v1.json")
NOTICE = Path("docs/assurance/THIRD_PARTY_AND_FORK_ATTRIBUTION.md")
SHA40 = re.compile(r"^[0-9a-f]{40}$")


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def read(path: str) -> str:
    p = Path(path)
    if not p.is_file():
        fail(f"missing file: {path}")
    return p.read_text(encoding="utf-8")


if not MANIFEST.is_file():
    fail("federated manifest missing")
if not NOTICE.is_file():
    fail("third-party attribution document missing")

m = json.loads(MANIFEST.read_text(encoding="utf-8"))
if m.get("schema") != "rafcodephi.federated-components/v1":
    fail("schema")
if m.get("claim_allowed") is not False:
    fail("claim_allowed must remain false")
policy = m.get("policy", {})
for key in (
    "single_license_flattening_allowed",
    "relicense_upstream_work_allowed",
    "copy_without_origin_path_commit_and_license_allowed",
    "fork_maintainer_is_original_author",
):
    if policy.get(key) is not False:
        fail(f"unsafe policy: {key}")
if policy.get("unknown_evidence_token") != "TOKEN_VAZIO":
    fail("TOKEN_VAZIO policy")

components = {c.get("id"): c for c in m.get("components", [])}
required_ids = {
    "termux-app-rafcodephi",
    "blake3-reference-fork",
    "vectras-vm-android",
    "qemu-rafaelia",
    "androidx-rmr",
    "termux-api-rafcodephi",
}
if set(components) != required_ids:
    fail(f"component set mismatch: {sorted(set(components) ^ required_ids)}")

for cid, c in components.items():
    for field in (
        "local_repository",
        "source_commit",
        "upstream_project",
        "upstream_repository",
        "original_authors_or_org",
        "license_expression",
        "license_evidence",
        "integration_mode",
        "copy_policy",
    ):
        if not c.get(field):
            fail(f"{cid}: missing {field}")
    if not SHA40.fullmatch(c["source_commit"]):
        fail(f"{cid}: source_commit is not exact 40-hex")
    if c.get("attribution_required") is not True:
        fail(f"{cid}: attribution must be required")
    if c.get("distribution_candidate") is True and c["upstream_repository"] == "TOKEN_VAZIO":
        fail(f"{cid}: distributable component has unknown upstream")

b3 = components["blake3-reference-fork"]
if b3["upstream_repository"] != "BLAKE3-team/BLAKE3":
    fail("BLAKE3 upstream")
if "Jack O'Connor" not in b3["original_authors_or_org"] or "Samuel Neves" not in b3["original_authors_or_org"]:
    fail("BLAKE3 authors")
if b3["package_binding"].get("source_policy") != "official_upstream_not_local_fork":
    fail("BLAKE3 package must not silently substitute local fork")

vectras = components["vectras-vm-android"]
if vectras["upstream_repository"] != "xoureldeen/Vectras-VM-Android":
    fail("Vectras upstream")
if "xoureldeen" not in vectras["original_authors_or_org"]:
    fail("Vectras original author attribution")

qemu = components["qemu-rafaelia"]
if qemu.get("file_license_policy") != "PATH_AWARE_REQUIRED":
    fail("QEMU must remain path-aware licensed")
if qemu["package_binding"].get("arm32_x86_64_guest_runtime") != "TOKEN_VAZIO":
    fail("ARM32 QEMU x86_64 runtime overclaim")

androidx = components["androidx-rmr"]
if androidx["license_expression"] != "Apache-2.0":
    fail("AndroidX root license")
if androidx["upstream_repository"] == "TOKEN_VAZIO" and "no_copy" not in androidx["copy_policy"]:
    fail("AndroidX unknown exact upstream cannot authorize copy")

termux_api = components["termux-api-rafcodephi"]
if termux_api["upstream_repository"] != "termux/termux-api":
    fail("Termux:API upstream")

# Existing official recipes remain authoritative for third-party source bytes.
b3_recipe = read("packages/b3sum/build.sh")
if "BLAKE3-team/BLAKE3" not in b3_recipe or 'TERMUX_PKG_LICENSE="CC0-1.0"' not in b3_recipe:
    fail("b3sum upstream/license drift")
qemu_recipe = read("packages/qemu-system-x86-64-headless/build.sh")
if "https://download.qemu.org/" not in qemu_recipe or 'TERMUX_PKG_LICENSE="GPL-2.0"' not in qemu_recipe:
    fail("QEMU upstream/license drift")

expected_profiles = {
    "bootstrap": (
        "rafcodephi-bootstrap-profile",
        "apt, bash, busybox, dpkg, ca-certificates, coreutils, termux-tools, rafcodephi-federation-metadata",
    ),
    "runtime": (
        "rafcodephi-runtime-profile",
        "rafcodephi-bootstrap-profile, openssl, curl, git, python, procps, b3sum",
    ),
    "vectras": (
        "rafcodephi-vectras-profile",
        "rafcodephi-runtime-profile, proot",
    ),
    "full": (
        "rafcodephi-full-profile",
        "rafcodephi-vectras-profile, termux-api",
    ),
}

for profile, (pkg, depends) in expected_profiles.items():
    declared = m["package_profiles"].get(profile, {})
    if declared.get("metapackage") != pkg:
        fail(f"{profile}: metapackage manifest drift")
    if declared.get("depends") != [x.strip() for x in depends.split(",")]:
        fail(f"{profile}: dependency manifest drift")
    recipe_path = f"packages/{pkg}/build.sh"
    recipe = read(recipe_path)
    if 'TERMUX_PKG_LICENSE="Apache-2.0"' not in recipe:
        fail(f"{pkg}: local glue license")
    if "TERMUX_PKG_SKIP_SRC_EXTRACT=true" not in recipe or "TERMUX_PKG_PLATFORM_INDEPENDENT=true" not in recipe:
        fail(f"{pkg}: metapackage flags")
    if f'TERMUX_PKG_DEPENDS="{depends}"' not in recipe:
        fail(f"{pkg}: dependency recipe drift")

metadata = read("packages/rafcodephi-federation-metadata/build.sh")
if "TERMUX_PKG_SRCURL" in metadata:
    fail("metadata package must not vendor external source")
for required_path in (
    "docs/assurance/rafcodephi-federated-components.v1.json",
    "docs/assurance/THIRD_PARTY_AND_FORK_ATTRIBUTION.md",
):
    if required_path not in metadata:
        fail(f"metadata package does not install {required_path}")

notice = NOTICE.read_text(encoding="utf-8")
for token in (
    "xoureldeen",
    "Jack O'Connor",
    "Samuel Neves",
    "Fabrice Bellard",
    "Termux team and contributors",
    "Origin-Repository",
    "Origin-Commit",
    "Origin-Path",
    "Governing-License",
):
    if token not in notice:
        fail(f"attribution notice missing token: {token}")

readme = read("README.md")
if "RAFCODEPHI provenance and third-party attribution" not in readme:
    fail("README provenance entry missing")

gaps = m.get("scoped_gaps", {})
if gaps.get("TV-VECTRAS-ARM32-QEMU-X86_64", {}).get("state") != "TOKEN_VAZIO":
    fail("Vectras ARM32 QEMU gap must remain explicit")
if gaps.get("TV-PACKAGE-LICENSE-INVENTORY-GLOBAL", {}).get("state") != "TOKEN_VAZIO":
    fail("global package license inventory must not be falsely closed")

print("PASS: RAFCODEPHI federation attribution, package profiles and copy boundaries are fail-closed")
