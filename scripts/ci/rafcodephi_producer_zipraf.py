#!/usr/bin/env python3
"""Deterministic, fail-closed RAFCODEPHI producer evidence ZIP.

Only explicitly authorized producer files are included. The original handoff
artifact remains unchanged for existing termux-app-rafacodephi consumers.
No APK, private keys, credentials, or arbitrary user directories are bundled.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import tempfile
import zipfile

SCHEMA = "rafcodephi.producer-zipraf/v1"
REQUIRED = (
    "artifacts/rafcodephi-bootstrap/RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt",
    "artifacts/rafcodephi-bootstrap/rafcodephi-bootstrap-arm.zip",
    "artifacts/rafcodephi-bootstrap/rafcodephi-bootstrap-aarch64.zip",
    "build/reports/rafcodephi-auto-handoff.json",
    "build/reports/rafcodephi-auto-handoff.json.sha256",
    "build/reports/rafcodephi-auto-bootstrap-status.json",
)
OPTIONAL = (
    "build/reports/rafcodephi-auto-properties.json",
    "build/reports/rafcodephi-auto-termux-packages.commit",
    "build/reports/rafcodephi-auto-bootstrap.log",
    "build/reports/rafcodephi-auto-bootstrap.sha256",
)
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def member(name: str, data: bytes) -> tuple[zipfile.ZipInfo, bytes]:
    if (name.lower().endswith(".apk") or name.startswith("/")
            or "\\" in name or ".." in name.split("/")):
        raise ValueError("unsafe_or_apk_member:" + name)
    info = zipfile.ZipInfo(name, ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3
    info.external_attr = 0o100644 << 16
    return info, data


def bundle(root: Path, output: Path, source_sha: str, run_id: str) -> dict:
    root = root.resolve(strict=True)
    payloads: dict[str, bytes] = {}
    for name in REQUIRED + OPTIONAL:
        path = root / name
        if path.is_symlink():
            raise ValueError("symlink_input_forbidden:" + name)
        if not path.exists():
            if name in REQUIRED:
                raise FileNotFoundError("required_producer_input:" + name)
            continue
        if not path.is_file() or not path.resolve().is_relative_to(root):
            raise ValueError("unsafe_input:" + name)
        data = path.read_bytes()
        if not data:
            raise ValueError("empty_producer_input:" + name)
        member(name, data)
        payloads[name] = data

    receipt = json.loads(payloads["build/reports/rafcodephi-auto-handoff.json"])
    if (receipt.get("schema") != "rafcodephi.termux-packages-producer-handoff/v1"
            or receipt.get("producer_commit") != source_sha
            or str(receipt.get("producer_run_id")) != run_id
            or receipt.get("claim_allowed") is not False
            or receipt.get("physical_android") != "TOKEN_VAZIO"):
        raise ValueError("handoff_receipt_identity_or_boundary_mismatch")
    expected = receipt.get("sha256", {})
    for name in REQUIRED[:3]:
        if expected.get(name) != sha256(payloads[name]):
            raise ValueError("producer_sha256_mismatch:" + name)

    index = {
        "schema": SCHEMA,
        "state": "BUILD_EVIDENCE_PASS",
        "source_sha": source_sha,
        "run_id": run_id,
        "package": "com.termux.rafacodephi",
        "apk_members": 0,
        "consumer_handoff_contract": "PRESERVED_SEPARATE",
        "physical_android": "TOKEN_VAZIO",
        "claim_allowed": False,
        "files": [
            {"path": n, "bytes": len(data), "sha256": sha256(data)}
            for n, data in sorted(payloads.items())
        ],
    }
    index_bytes = (json.dumps(index, indent=2, sort_keys=True) + "\n").encode()
    payloads["ZIPRAF_MANIFEST.json"] = index_bytes

    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9, strict_timestamps=True) as archive:
        for name, data in sorted(payloads.items()):
            info, data = member(name, data)
            archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    verify(output, index)
    output.with_suffix(output.suffix + ".sha256").write_text(
        sha256(output.read_bytes()) + "  " + output.name + "\n", encoding="utf-8"
    )
    return index


def verify(path: Path, expected: dict | None = None) -> dict:
    with zipfile.ZipFile(path, "r") as archive:
        if archive.testzip() is not None:
            raise ValueError("zip_crc_failure")
        infos = archive.infolist()
        names = [i.filename for i in infos]
        if (len(names) != len(set(names)) or names != sorted(names)
                or any(i.is_dir() or i.filename.lower().endswith(".apk") for i in infos)):
            raise ValueError("zip_duplicate_order_or_apk")
        index = json.loads(archive.read("ZIPRAF_MANIFEST.json"))
        if expected is not None and index != expected:
            raise ValueError("manifest_mismatch")
        if index.get("schema") != SCHEMA or index.get("claim_allowed") is not False:
            raise ValueError("zip_schema_or_claim_mismatch")
        entries = index.get("files")
        if not isinstance(entries, list) or len(entries) + 1 != len(names):
            raise ValueError("zip_inventory_count_mismatch")
        if set(names) != {"ZIPRAF_MANIFEST.json"} | {x["path"] for x in entries}:
            raise ValueError("zip_inventory_identity_mismatch")
        for x in entries:
            raw = archive.read(x["path"])
            if len(raw) != x["bytes"] or sha256(raw) != x["sha256"]:
                raise ValueError("zip_member_hash_mismatch:" + x["path"])
        return index


def self_test() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        for name in REQUIRED + OPTIONAL:
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture\n")
        sha = "a" * 40
        run_id = "4242"
        data = {
            "schema": "rafcodephi.termux-packages-producer-handoff/v1",
            "producer_commit": sha,
            "producer_run_id": 4242,
            "claim_allowed": False,
            "physical_android": "TOKEN_VAZIO",
            "sha256": {
                name: sha256((root / name).read_bytes()) for name in REQUIRED[:3]
            },
        }
        (root / "build/reports/rafcodephi-auto-handoff.json").write_text(
            json.dumps(data), encoding="utf-8"
        )
        out = root / "bundle.zip"
        first = bundle(root, out, sha, run_id)
        first_digest = sha256(out.read_bytes())
        bundle(root, out, sha, run_id)
        assert sha256(out.read_bytes()) == first_digest
        assert len(verify(out)["files"]) == len(REQUIRED + OPTIONAL)
        assert first["apk_members"] == 0
        (root / REQUIRED[0]).write_bytes(b"tampered")
        try:
            bundle(root, out, sha, run_id)
        except ValueError as error:
            assert "producer_sha256_mismatch" in str(error)
        else:
            raise AssertionError("mismatched producer bytes accepted")
        try:
            member("dist/app-release.apk", b"not-allowed")
        except ValueError as error:
            assert "apk_member" in str(error)
        else:
            raise AssertionError("APK member accepted")
        print("PRODUCER_ZIPRAF_SELFTEST=PASS deterministic=PASS hash_mismatch=REJECTED apk=REJECTED")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--output", type=Path,
                        default=Path("build/reports/rafcodephi-producer-evidence.zip"))
    parser.add_argument("--source-sha", default=os.environ.get("GITHUB_SHA", "TOKEN_VAZIO"))
    parser.add_argument("--run-id", default=os.environ.get("GITHUB_RUN_ID", "TOKEN_VAZIO"))
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if len(args.source_sha) != 40 or not all(c in "0123456789abcdef" for c in args.source_sha):
        raise SystemExit("source_sha_unverified")
    if not args.run_id.isdecimal():
        raise SystemExit("run_id_unverified")
    result = bundle(args.root, args.output, args.source_sha, args.run_id)
    print("PRODUCER_ZIPRAF=PASS files={0} path={1}".format(len(result["files"]), args.output))


if __name__ == "__main__":
    main()
