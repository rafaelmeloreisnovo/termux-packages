#!/usr/bin/env python3
"""RAFCODEPHI producer artifact: bounded parallel ARM/AArch64 verification.

Build once; verify each ABI in an independent matrix job; join only after both
typed receipts have passed. Python standard library; no device/runtime claims.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import tempfile
import zipfile
from pathlib import Path

SCHEMA = "rafcodephi.producer-arch-batch/v1"
JOIN_SCHEMA = "rafcodephi.producer-arch-batch-join/v1"
ARCHES = ("arm", "aarch64")
MANIFEST = "artifacts/rafcodephi-bootstrap/RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt"
HANDOFF = "build/reports/rafcodephi-auto-handoff.json"
REQUIRED_ZIP_ENTRIES = ("BOOTSTRAP_PROFILE.json", "SYMLINKS.txt")
HEX40 = re.compile(r"^[0-9a-f]{40}$")
HEX64 = re.compile(r"^[0-9a-f]{64}$")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def bounded_read(path: Path, maximum: int) -> bytes:
    if path.is_symlink() or not path.is_file():
        raise ValueError("unsafe_or_missing_input:" + str(path))
    size = path.stat().st_size
    if not 0 < size <= maximum:
        raise ValueError("invalid_input_size:" + str(path))
    return path.read_bytes()


def read_json(path: Path, maximum: int = 1024 * 1024) -> dict:
    result = json.loads(bounded_read(path, maximum))
    if not isinstance(result, dict):
        raise ValueError("object_required:" + str(path))
    return result


def identity(sha: str, run: str, artifact_id: str, artifact_digest: str) -> None:
    if not HEX40.fullmatch(sha):
        raise ValueError("source_sha_invalid")
    if not run.isdecimal() or not artifact_id.isdecimal():
        raise ValueError("run_or_artifact_id_invalid")
    if not HEX64.fullmatch(artifact_digest):
        raise ValueError("artifact_digest_invalid")


def verify_arch(inbound: Path, arch: str, sha: str, run: str,
                artifact_id: str, artifact_digest: str) -> dict:
    identity(sha, run, artifact_id, artifact_digest)
    if arch not in ARCHES:
        raise ValueError("unknown_architecture")
    root = inbound.resolve(strict=True)
    receipt = read_json(root / HANDOFF)
    if (receipt.get("schema") != "rafcodephi.termux-packages-producer-handoff/v1"
            or receipt.get("producer_commit") != sha
            or str(receipt.get("producer_run_id")) != run
            or receipt.get("architectures") != ["arm", "aarch64"]
            or receipt.get("claim_allowed") is not False
            or receipt.get("physical_android") != "TOKEN_VAZIO"):
        raise ValueError("producer_handoff_contract_mismatch")
    expected = receipt.get("sha256")
    if not isinstance(expected, dict):
        raise ValueError("producer_hash_manifest_missing")
    manifest_data = bounded_read(root / MANIFEST, 1024 * 1024)
    if expected.get(MANIFEST) != digest(manifest_data):
        raise ValueError("manifest_sha_mismatch")
    if (b"schema=rafcodephi.real-bootstrap-sourcebuild/v1\n" not in manifest_data
            or b"package_name=com.termux.rafacodephi\n" not in manifest_data
            or b"device_runtime_proof=TOKEN_VAZIO\n" not in manifest_data):
        raise ValueError("bootstrap_manifest_contract_mismatch")

    member_name = "artifacts/rafcodephi-bootstrap/rafcodephi-bootstrap-" + arch + ".zip"
    raw = bounded_read(root / member_name, 512 * 1024 * 1024)
    if expected.get(member_name) != digest(raw):
        raise ValueError("bootstrap_hash_mismatch")
    with zipfile.ZipFile(root / member_name, "r") as zipped:
        if zipped.testzip() is not None:
            raise ValueError("bootstrap_zip_crc_failure")
        names = zipped.namelist()
        if len(names) != len(set(names)) or any(x not in names for x in REQUIRED_ZIP_ENTRIES):
            raise ValueError("bootstrap_zip_entries_invalid")
        profile = json.loads(zipped.read("BOOTSTRAP_PROFILE.json"))
        if (profile.get("schema") != "rafcodephi-bootstrap-profile/v1"
                or profile.get("arch") != arch
                or profile.get("profile") != "real-pkg"
                or profile.get("package_name") != "com.termux.rafacodephi"
                or profile.get("claim_allowed") is not False
                or profile.get("release_allowed") is not False
                or profile.get("device_validation") != "TOKEN_VAZIO"):
            raise ValueError("bootstrap_profile_contract_mismatch")
    return {
        "schema": SCHEMA,
        "state": "PASS",
        "scope": "PRODUCER_ARCHIVE_BYTE_AND_PROFILE_ONLY",
        "arch": arch,
        "source_sha": sha,
        "run_id": run,
        "artifact_id": artifact_id,
        "artifact_digest": artifact_digest,
        "manifest_sha256": digest(manifest_data),
        "bootstrap_sha256": digest(raw),
        "bootstrap_bytes": len(raw),
        "physical_android": "TOKEN_VAZIO",
        "claim_allowed": False,
    }


def join(receipt_dir: Path, sha: str, run: str,
         artifact_id: str, artifact_digest: str) -> dict:
    identity(sha, run, artifact_id, artifact_digest)
    receipts = []
    for arch in ARCHES:
        path = receipt_dir / ("batch-" + arch + ".json")
        d = read_json(path)
        if (d.get("schema") != SCHEMA or d.get("state") != "PASS"
                or d.get("scope") != "PRODUCER_ARCHIVE_BYTE_AND_PROFILE_ONLY"
                or d.get("arch") != arch or d.get("source_sha") != sha
                or str(d.get("run_id")) != run
                or str(d.get("artifact_id")) != artifact_id
                or d.get("artifact_digest") != artifact_digest
                or not HEX64.fullmatch(str(d.get("manifest_sha256", "")))
                or not HEX64.fullmatch(str(d.get("bootstrap_sha256", "")))
                or not isinstance(d.get("bootstrap_bytes"), int)
                or d["bootstrap_bytes"] <= 0
                or d.get("physical_android") != "TOKEN_VAZIO"
                or d.get("claim_allowed") is not False):
            raise ValueError("batch_receipt_mismatch:" + arch)
        receipts.append(d)
    if receipts[0]["manifest_sha256"] != receipts[1]["manifest_sha256"]:
        raise ValueError("cross_arch_manifest_mismatch")
    # Note: distinct architecture does not require distinct bytes by
    # cryptographic necessity. Structural validation is performed in verify_arch.
    return {
        "schema": JOIN_SCHEMA,
        "state": "PASS",
        "scope": "BOTH_PRODUCER_ARCHIVE_BATCHES_ONLY",
        "architectures": list(ARCHES),
        "source_sha": sha,
        "run_id": run,
        "artifact_id": artifact_id,
        "artifact_digest": artifact_digest,
        "manifest_sha256": receipts[0]["manifest_sha256"],
        "receipts_sha256": {
            arch: digest(bounded_read(receipt_dir / ("batch-" + arch + ".json"), 1024 * 1024))
            for arch in ARCHES
        },
        "physical_android": "TOKEN_VAZIO",
        "claim_allowed": False,
    }


def write_receipt(path: Path, obj: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() or path.is_symlink():
        raise ValueError("receipt_overwrite_forbidden:" + str(path))
    encoded = (json.dumps(obj, indent=2, sort_keys=True) + "\n").encode()
    with path.open("xb") as out:
        out.write(encoded)


def self_test() -> None:
    sha, run, artifact_id, artifact_digest = "a" * 40, "123", "456", "b" * 64
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        manifest = (b"schema=rafcodephi.real-bootstrap-sourcebuild/v1\n"
                    b"package_name=com.termux.rafacodephi\n"
                    b"device_runtime_proof=TOKEN_VAZIO\n")
        p = root / MANIFEST
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(manifest)
        hashes = {MANIFEST: digest(manifest)}
        for arch in ARCHES:
            target = root / ("artifacts/rafcodephi-bootstrap/rafcodephi-bootstrap-" + arch + ".zip")
            with zipfile.ZipFile(target, "w") as z:
                z.writestr("SYMLINKS.txt", "bin/sh")
                z.writestr("BOOTSTRAP_PROFILE.json", json.dumps({
                    "schema": "rafcodephi-bootstrap-profile/v1",
                    "arch": arch, "profile": "real-pkg",
                    "package_name": "com.termux.rafacodephi",
                    "claim_allowed": False, "release_allowed": False,
                    "device_validation": "TOKEN_VAZIO"
                }))
            hashes[str(target.relative_to(root))] = digest(target.read_bytes())
        handoff = {
            "schema": "rafcodephi.termux-packages-producer-handoff/v1",
            "producer_commit": sha, "producer_run_id": 123,
            "architectures": list(ARCHES),
            "claim_allowed": False, "physical_android": "TOKEN_VAZIO",
            "sha256": hashes,
        }
        h = root / HANDOFF
        h.parent.mkdir(parents=True, exist_ok=True)
        h.write_text(json.dumps(handoff), encoding="utf-8")
        receipts = root / "out"
        for arch in ARCHES:
            result = verify_arch(root, arch, sha, run, artifact_id, artifact_digest)
            write_receipt(receipts / ("batch-" + arch + ".json"), result)
        assert join(receipts, sha, run, artifact_id, artifact_digest)["state"] == "PASS"
        # Test wrong version/architecture and byte tampering fail-closed.
        try:
            verify_arch(root, "x86", sha, run, artifact_id, artifact_digest)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid arch accepted")
        zip_path = root / "artifacts/rafcodephi-bootstrap/rafcodephi-bootstrap-arm.zip"
        zip_path.write_bytes(zip_path.read_bytes() + b"tamper")
        try:
            verify_arch(root, "arm", sha, run, artifact_id, artifact_digest)
        except ValueError as error:
            assert "bootstrap_hash_mismatch" in str(error)
        else:
            raise AssertionError("changed archive accepted")
        (receipts / "batch-aarch64.json").unlink()
        try:
            join(receipts, sha, run, artifact_id, artifact_digest)
        except ValueError:
            pass
        else:
            raise AssertionError("missing batch accepted")
    print("RAFCODEPHI_PARALLEL_BATCH_SELFTEST=PASS two_arches=PASS tamper=REJECTED join_missing=REJECTED")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--mode", choices=("verify", "join"))
    parser.add_argument("--arch", choices=ARCHES)
    parser.add_argument("--inbound", type=Path, default=Path("inbound"))
    parser.add_argument("--receipts", type=Path, default=Path("batch-receipts"))
    parser.add_argument("--output", type=Path)
    parser.add_argument("--source-sha", default="")
    parser.add_argument("--run-id", default="")
    parser.add_argument("--artifact-id", default="")
    parser.add_argument("--artifact-digest", default="")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.mode == "verify" and not args.arch:
        parser.error("--arch required for verify")
    if args.mode is None or args.output is None:
        parser.error("--mode and --output required")
    if args.mode == "verify":
        result = verify_arch(args.inbound, args.arch, args.source_sha,
                             args.run_id, args.artifact_id, args.artifact_digest)
    else:
        result = join(args.receipts, args.source_sha, args.run_id,
                      args.artifact_id, args.artifact_digest)
    write_receipt(args.output, result)
    print("RAFCODEPHI_BATCH_GATE=PASS mode=" + args.mode +
          " scope=" + result["scope"])


if __name__ == "__main__":
    main()
