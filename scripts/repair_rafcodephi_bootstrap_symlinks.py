#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import tarfile
import tempfile
import zipfile
from pathlib import Path

SCHEMA = "rafcodephi.bootstrap-symlink-evidence-repair/v1"
EXPECTED = {
    "bin/sh": "dash",
    "libexec/termux-api": "termux-api-broadcast",
}


class RepairError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def normalize_link(link: str) -> str:
    if link.startswith("/") or "\\" in link:
        raise RepairError(f"unsafe symlink destination: {link!r}")
    while link.startswith("./"):
        link = link[2:]
    parts = link.split("/")
    if not link or any(part in ("", ".", "..") for part in parts):
        raise RepairError(f"unsafe symlink destination: {link!r}")
    return link


def parse_symlinks(payload: bytes) -> dict[str, str]:
    mapping: dict[str, str] = {}
    for number, raw in enumerate(payload.decode("utf-8").splitlines(), 1):
        if not raw:
            continue
        parts = raw.split("←")
        if len(parts) != 2 or not parts[0] or not parts[1]:
            raise RepairError(f"malformed SYMLINKS.txt line {number}: {raw!r}")
        target, link = parts
        link = normalize_link(link)
        prior = mapping.get(link)
        if prior is not None and prior != target:
            raise RepairError(
                f"conflicting symlink destination {link}: {prior!r} vs {target!r}"
            )
        mapping[link] = target
    return mapping


def iter_deb_symlinks(deb: Path):
    proc = subprocess.Popen(
        ["dpkg-deb", "--fsys-tarfile", str(deb)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    assert proc.stdout is not None
    try:
        with tarfile.open(fileobj=proc.stdout, mode="r|*") as tf:
            for member in tf:
                if member.issym():
                    name = member.name
                    while name.startswith("./"):
                        name = name[2:]
                    yield name, member.linkname
    finally:
        proc.stdout.close()
        stderr = (
            proc.stderr.read().decode("utf-8", "replace") if proc.stderr else ""
        )
        rc = proc.wait()
        if rc != 0:
            raise RepairError(
                f"dpkg-deb failed for {deb}: exit={rc}: {stderr.strip()}"
            )


def prove_from_debs(
    deb_dir: Path, prefix: str, rel: str, target: str
) -> dict[str, str]:
    expected_member = f"{prefix.strip('/')}/{rel}"
    matches = []
    for deb in sorted(deb_dir.glob("*.deb")):
        for name, link_target in iter_deb_symlinks(deb):
            if name == expected_member:
                matches.append((deb, link_target))

    if not matches:
        raise RepairError(f"no .deb proves required symlink {rel} -> {target}")

    exact = [(deb, got) for deb, got in matches if got == target]
    wrong = [(deb, got) for deb, got in matches if got != target]
    if wrong:
        details = ", ".join(f"{deb.name}:{got}" for deb, got in wrong)
        raise RepairError(f"conflicting .deb symlink proof for {rel}: {details}")
    if len(exact) != 1:
        details = ", ".join(deb.name for deb, _ in exact)
        raise RepairError(f"ambiguous .deb symlink proof for {rel}: {details}")

    deb, _ = exact[0]
    return {
        "deb": deb.name,
        "deb_sha256": sha256(deb),
        "member": expected_member,
        "target": target,
    }


def rewrite_symlinks(zip_path: Path, new_payload: bytes) -> None:
    fd, tmp_name = tempfile.mkstemp(
        prefix=zip_path.name + ".repair.", dir=zip_path.parent
    )
    os.close(fd)
    tmp = Path(tmp_name)
    try:
        with zipfile.ZipFile(zip_path, "r") as source, zipfile.ZipFile(
            tmp, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
        ) as target:
            seen = False
            for info in source.infolist():
                payload = source.read(info.filename)
                if info.filename == "SYMLINKS.txt":
                    if seen:
                        raise RepairError("duplicate SYMLINKS.txt in bootstrap")
                    payload = new_payload
                    seen = True
                target.writestr(info, payload)

            if not seen:
                info = zipfile.ZipInfo("SYMLINKS.txt")
                info.date_time = (1980, 1, 1, 0, 0, 0)
                info.external_attr = 0o100600 << 16
                target.writestr(info, new_payload)

        os.replace(tmp, zip_path)
    finally:
        if tmp.exists():
            tmp.unlink()


def repair(zip_path: Path, deb_dir: Path, prefix: str) -> dict:
    if not zip_path.is_file():
        raise RepairError(f"bootstrap zip missing: {zip_path}")
    if not deb_dir.is_dir():
        raise RepairError(f"deb evidence dir missing: {deb_dir}")
    if not list(deb_dir.glob("*.deb")):
        raise RepairError(f"deb evidence dir contains no .deb: {deb_dir}")

    before = sha256(zip_path)
    with zipfile.ZipFile(zip_path, "r") as zf:
        names = zf.namelist()
        payload = zf.read("SYMLINKS.txt") if "SYMLINKS.txt" in names else b""

    mapping = parse_symlinks(payload)
    proofs = []
    repairs = []

    for rel, expected_target in EXPECTED.items():
        observed = mapping.get(rel)
        if observed is not None:
            if observed != expected_target:
                raise RepairError(
                    f"bootstrap symlink conflict {rel}: "
                    f"observed={observed!r} expected={expected_target!r}"
                )
            proofs.append(
                {"path": rel, "source": "bootstrap", "target": observed}
            )
            continue

        proof = prove_from_debs(deb_dir, prefix, rel, expected_target)
        proofs.append({"path": rel, "source": "deb", **proof})
        mapping[rel] = expected_target
        repairs.append(
            {
                "path": rel,
                "target": expected_target,
                "proof_deb": proof["deb"],
                "proof_sha256": proof["deb_sha256"],
            }
        )

    if repairs:
        original_lines = [
            line for line in payload.decode("utf-8").splitlines() if line
        ]
        additions = [
            f"{EXPECTED[item['path']]}←./{item['path']}" for item in repairs
        ]
        new_payload = (
            "\n".join(original_lines + additions) + "\n"
        ).encode("utf-8")
        rewrite_symlinks(zip_path, new_payload)

    after = sha256(zip_path)
    with zipfile.ZipFile(zip_path, "r") as zf:
        final = parse_symlinks(zf.read("SYMLINKS.txt"))
    for rel, target in EXPECTED.items():
        if final.get(rel) != target:
            raise RepairError(
                f"postcondition failed: {rel} -> {final.get(rel)!r}, "
                f"expected {target!r}"
            )

    return {
        "schema": SCHEMA,
        "state": "PASS",
        "claim_allowed": False,
        "device_runtime_proof": "TOKEN_VAZIO",
        "prefix": prefix,
        "bootstrap": zip_path.name,
        "bootstrap_sha256_before": before,
        "bootstrap_sha256_after": after,
        "mutation_performed": bool(repairs),
        "repairs": repairs,
        "proofs": proofs,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--zip", required=True, type=Path)
    parser.add_argument("--deb-dir", required=True, type=Path)
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--receipt", required=True, type=Path)
    args = parser.parse_args()

    try:
        receipt = repair(args.zip, args.deb_dir, args.prefix)
    except (
        RepairError,
        OSError,
        zipfile.BadZipFile,
        subprocess.SubprocessError,
    ) as exc:
        print(
            f"RAFCODEPHI_BOOTSTRAP_SYMLINK_REPAIR=BLOCKED reason={exc}",
            file=__import__("sys").stderr,
        )
        return 1

    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(
        "RAFCODEPHI_BOOTSTRAP_SYMLINK_REPAIR=PASS "
        f"mutation={str(receipt['mutation_performed']).lower()} "
        f"repairs={len(receipt['repairs'])} claim_allowed=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
