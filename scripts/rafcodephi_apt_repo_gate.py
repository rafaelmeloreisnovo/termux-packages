#!/usr/bin/env python3
"""D4 APT repository metadata gate for RAFCODEPHI.

The gate accepts only Debian artifacts whose SHA-256 is bound to a successful D3
artifact receipt. It then creates deterministic Debian repository metadata and
validates the generated hashes/indices.

Scope:
  PASS => repository metadata is internally consistent for the validated .deb set.
  NOT claimed => repository signature/trust, remote hosting, apt update/install,
                 bootstrap integration, or physical Android runtime.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import lzma
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

SCHEMA = "rafcodephi_apt_repo_gate/1.0.0"
D3_CI_SCHEMA = "rafcodephi_d3_ci_receipt/1.0.0"
D3_ARTIFACT_SCHEMAS = {
    "rafcodephi_delivery_gate/1.0.0",
    "rafcodephi_delivery_gate/1.1.0",
}
ALLOWED_ARCHES = {"arm", "aarch64"}


class GateError(RuntimeError):
    pass


@dataclass(frozen=True)
class DebRecord:
    source: Path
    package: str
    version: str
    architecture: str
    sha256: str
    size: int
    control: str
    receipt: Path


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run_text(argv: list[str]) -> str:
    try:
        return subprocess.check_output(argv, text=True, stderr=subprocess.STDOUT)
    except (OSError, subprocess.CalledProcessError) as exc:
        raise GateError(f"command failed: {' '.join(argv)}: {exc}") from exc


def dpkg_field(deb: Path, field: str) -> str:
    return run_text(["dpkg-deb", "-f", str(deb), field]).strip()


def dpkg_control(deb: Path) -> str:
    text = run_text(["dpkg-deb", "-f", str(deb)]).replace("\r\n", "\n").strip()
    if not text:
        raise GateError(f"empty control metadata: {deb}")
    return text + "\n"


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise GateError(f"invalid receipt JSON {path}: {exc}") from exc


def extract_artifact_binding(doc: Any) -> dict[str, Any] | None:
    if not isinstance(doc, dict):
        return None
    schema = doc.get("schema")

    if schema == D3_CI_SCHEMA:
        if doc.get("d3") != "PASS" or doc.get("claim_allowed") is not False:
            return None
        art = doc.get("artifact_gate")
        if not isinstance(art, dict) or art.get("state") != "PASS":
            return None
        return art

    if schema in D3_ARTIFACT_SCHEMAS and doc.get("gate") == "artifact":
        if doc.get("state") != "PASS" or doc.get("claim_allowed") is not False:
            return None
        return doc

    return None


def receipt_index(receipts_dir: Path) -> dict[str, tuple[Path, dict[str, Any]]]:
    if not receipts_dir.is_dir():
        raise GateError(f"receipts directory missing: {receipts_dir}")
    out: dict[str, tuple[Path, dict[str, Any]]] = {}
    for path in sorted(receipts_dir.rglob("*.json")):
        doc = load_json(path)
        binding = extract_artifact_binding(doc)
        if binding is None:
            continue
        digest = binding.get("artifact_sha256")
        if not isinstance(digest, str) or len(digest) != 64:
            raise GateError(f"receipt has invalid artifact_sha256: {path}")
        try:
            int(digest, 16)
        except ValueError as exc:
            raise GateError(f"receipt has non-hex artifact_sha256: {path}") from exc
        if digest in out:
            raise GateError(f"duplicate D3 receipt binding for sha256={digest}")
        out[digest] = (path, binding)
    return out


def collect_artifacts(artifacts_dir: Path, receipts_dir: Path, arch: str) -> list[DebRecord]:
    if arch not in ALLOWED_ARCHES:
        raise GateError(f"unsupported architecture: {arch}")
    if not artifacts_dir.is_dir():
        raise GateError(f"artifacts directory missing: {artifacts_dir}")
    if shutil.which("dpkg-deb") is None:
        raise GateError("dpkg-deb unavailable")

    receipts = receipt_index(receipts_dir)
    debs = sorted(p for p in artifacts_dir.rglob("*.deb") if p.is_file())
    if not debs:
        raise GateError("no .deb artifacts found; D4 cannot be promoted from D3 absence")

    records: list[DebRecord] = []
    identities: set[tuple[str, str, str]] = set()
    for deb in debs:
        digest = sha256_file(deb)
        receipt_pair = receipts.get(digest)
        if receipt_pair is None:
            raise GateError(f"artifact lacks matching PASS D3 receipt: {deb.name} sha256={digest}")
        receipt_path, binding = receipt_pair

        package = dpkg_field(deb, "Package")
        version = dpkg_field(deb, "Version")
        architecture = dpkg_field(deb, "Architecture")
        if not package or not version or not architecture:
            raise GateError(f"required Debian metadata missing: {deb}")
        if architecture not in {arch, "all"}:
            raise GateError(
                f"artifact architecture mismatch: {deb.name}: {architecture!r} not {arch!r}/'all'"
            )

        # Bind receipt metadata to the actual artifact metadata when fields exist.
        for key, actual in (("package", package), ("version", version)):
            claimed = binding.get(key)
            if claimed is not None and claimed != actual:
                raise GateError(
                    f"D3 receipt metadata mismatch for {deb.name}: {key}={claimed!r} != {actual!r}"
                )
        claimed_arch = binding.get("debian_architecture") or binding.get("arch")
        if claimed_arch is not None and claimed_arch not in {architecture, arch}:
            raise GateError(
                f"D3 receipt architecture mismatch for {deb.name}: {claimed_arch!r}"
            )

        identity = (package, version, architecture)
        if identity in identities:
            raise GateError(f"duplicate package/version/architecture identity: {identity}")
        identities.add(identity)
        records.append(
            DebRecord(
                source=deb,
                package=package,
                version=version,
                architecture=architecture,
                sha256=digest,
                size=deb.stat().st_size,
                control=dpkg_control(deb),
                receipt=receipt_path,
            )
        )

    return sorted(records, key=lambda r: (r.package, r.version, r.architecture, r.source.name))


def strip_generated_fields(control: str) -> str:
    generated = {"Filename", "Size", "MD5sum", "SHA1", "SHA256", "SHA512"}
    lines = control.rstrip("\n").splitlines()
    out: list[str] = []
    skipping = False
    for line in lines:
        if line.startswith((" ", "\t")):
            if not skipping:
                out.append(line)
            continue
        field = line.split(":", 1)[0] if ":" in line else ""
        skipping = field in generated
        if not skipping:
            out.append(line)
    return "\n".join(out).rstrip() + "\n"


def package_paragraph(record: DebRecord, filename: str) -> str:
    control = strip_generated_fields(record.control)
    return (
        control
        + f"Filename: {filename}\n"
        + f"Size: {record.size}\n"
        + f"SHA256: {record.sha256}\n"
    )


def write_deterministic_gzip(path: Path, data: bytes) -> None:
    path.write_bytes(gzip.compress(data, compresslevel=9, mtime=0))


def write_deterministic_xz(path: Path, data: bytes) -> None:
    path.write_bytes(lzma.compress(data, format=lzma.FORMAT_XZ, preset=9, check=lzma.CHECK_CRC64))


def release_date(epoch: int) -> str:
    if epoch < 0:
        raise GateError("epoch must be non-negative")
    return datetime.fromtimestamp(epoch, tz=timezone.utc).strftime("%a, %d %b %Y %H:%M:%S +0000")


def rel_hash_line(path: Path, root: Path) -> str:
    rel = path.relative_to(root).as_posix()
    return f" {sha256_file(path)} {path.stat().st_size:16d} {rel}"


def generate_repo(
    records: Iterable[DebRecord], out_dir: Path, arch: str, suite: str, component: str, epoch: int
) -> dict[str, Any]:
    records = list(records)
    if not records:
        raise GateError("cannot generate repository from empty validated artifact set")
    if "/" in suite or "/" in component or not suite or not component:
        raise GateError("suite/component must be single non-empty path segments")

    if out_dir.exists():
        shutil.rmtree(out_dir)
    pool = out_dir / "pool" / component
    binary_dir = out_dir / "dists" / suite / component / f"binary-{arch}"
    pool.mkdir(parents=True)
    binary_dir.mkdir(parents=True)

    paragraphs: list[str] = []
    artifact_receipts: list[dict[str, Any]] = []
    for record in records:
        dest = pool / record.source.name
        if dest.exists():
            raise GateError(f"artifact filename collision: {dest.name}")
        shutil.copyfile(record.source, dest)
        if sha256_file(dest) != record.sha256:
            raise GateError(f"artifact copy hash mismatch: {dest}")
        rel = dest.relative_to(out_dir).as_posix()
        paragraphs.append(package_paragraph(record, rel))
        artifact_receipts.append(
            {
                "package": record.package,
                "version": record.version,
                "architecture": record.architecture,
                "filename": rel,
                "size": record.size,
                "sha256": record.sha256,
                "d3_receipt": str(record.receipt),
            }
        )

    packages_data = ("\n".join(p.rstrip() for p in paragraphs) + "\n").encode("utf-8")
    packages = binary_dir / "Packages"
    packages.write_bytes(packages_data)
    packages_gz = binary_dir / "Packages.gz"
    write_deterministic_gzip(packages_gz, packages_data)
    packages_xz = binary_dir / "Packages.xz"
    write_deterministic_xz(packages_xz, packages_data)

    release = out_dir / "dists" / suite / "Release"
    indexed = [packages, packages_gz, packages_xz]
    release_text = "\n".join(
        [
            "Origin: RAFCODEPHI",
            "Label: RAFCODEPHI",
            f"Suite: {suite}",
            f"Codename: {suite}",
            f"Date: {release_date(epoch)}",
            f"Architectures: {arch}",
            f"Components: {component}",
            "Description: RAFCODEPHI validated package repository metadata",
            "SHA256:",
            *[rel_hash_line(p, release.parent) for p in indexed],
            "",
        ]
    )
    release.write_text(release_text, encoding="utf-8")

    return {
        "schema": SCHEMA,
        "gate": "D4_REPOSITORY_METADATA",
        "state": "PASS",
        "claim_allowed": False,
        "suite": suite,
        "component": component,
        "architecture": arch,
        "artifact_count": len(records),
        "artifacts": artifact_receipts,
        "indices": {
            p.relative_to(out_dir).as_posix(): {
                "size": p.stat().st_size,
                "sha256": sha256_file(p),
            }
            for p in [packages, packages_gz, packages_xz, release]
        },
        "signature": "NOT_MEASURED",
        "remote_hosting": "NOT_MEASURED",
        "apt_update": "NOT_MEASURED",
        "apt_install": "NOT_MEASURED",
        "bootstrap": "NOT_MEASURED",
        "physical_device_runtime": "NOT_MEASURED",
        "next_gate": "validate signed/trusted repository policy and run apt metadata consumption in an isolated client before Android promotion",
    }


def validate_generated_repo(out_dir: Path, receipt: dict[str, Any]) -> None:
    indices = receipt.get("indices")
    if not isinstance(indices, dict) or not indices:
        raise GateError("D4 receipt has no indices")
    for rel, meta in indices.items():
        path = out_dir / rel
        if not path.is_file():
            raise GateError(f"generated metadata missing: {rel}")
        if meta.get("size") != path.stat().st_size:
            raise GateError(f"generated metadata size drift: {rel}")
        if meta.get("sha256") != sha256_file(path):
            raise GateError(f"generated metadata hash drift: {rel}")

    packages_rel = next((r for r in indices if r.endswith("/Packages")), None)
    if packages_rel is None:
        raise GateError("Packages index absent")
    text = (out_dir / packages_rel).read_text(encoding="utf-8")
    paragraphs = [p for p in text.strip().split("\n\n") if p.strip()]
    if len(paragraphs) != receipt.get("artifact_count"):
        raise GateError("Packages paragraph count does not match artifact_count")
    for p in paragraphs:
        for field in ("Package:", "Version:", "Architecture:", "Filename:", "Size:", "SHA256:"):
            if not any(line.startswith(field) for line in p.splitlines()):
                raise GateError(f"Packages paragraph missing {field}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--artifacts-dir", type=Path, required=True)
    ap.add_argument("--receipts-dir", type=Path, required=True)
    ap.add_argument("--out-dir", type=Path, required=True)
    ap.add_argument("--arch", choices=sorted(ALLOWED_ARCHES), required=True)
    ap.add_argument("--suite", default="stable")
    ap.add_argument("--component", default="main")
    ap.add_argument("--epoch", type=int, required=True)
    ap.add_argument("--receipt", type=Path)
    args = ap.parse_args()

    try:
        records = collect_artifacts(args.artifacts_dir, args.receipts_dir, args.arch)
        receipt = generate_repo(records, args.out_dir, args.arch, args.suite, args.component, args.epoch)
        validate_generated_repo(args.out_dir, receipt)
    except GateError as exc:
        print(f"D4_GATE=BLOCKED reason={exc}", file=sys.stderr)
        print("CLAIM_ALLOWED=false", file=sys.stderr)
        return 2

    text = json.dumps(receipt, indent=2, sort_keys=True) + "\n"
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    print("D4_REPOSITORY_METADATA=PASS", file=sys.stderr)
    print("D4_SIGNATURE=NOT_MEASURED", file=sys.stderr)
    print("D7_APT_RUNTIME=NOT_MEASURED", file=sys.stderr)
    print("CLAIM_ALLOWED=false", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
