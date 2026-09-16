#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for block in iter(lambda: fh.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def git_blob(root: Path, path: Path) -> str:
    rel = path.relative_to(root).as_posix()
    return subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", f"HEAD:{rel}"], text=True
    ).strip()


def deb_field(path: Path, field: str) -> str:
    return subprocess.check_output(
        ["dpkg-deb", "-f", str(path), field], text=True
    ).strip()


def recipe_literal(text: str, field: str) -> str:
    prefix = field + "="
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped.startswith(prefix):
            continue
        value = stripped[len(prefix):].strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
            value = value[1:-1]
        return value
    return f"TOKEN_VAZIO_{field}"


def recipe_source_contract(text: str) -> dict:
    urls = []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("TERMUX_PKG_SRCURL="):
            urls.append(stripped.split("=", 1)[1])
    checksums = re.findall(r"(?m)^\s*TERMUX_PKG_SHA256=(['\"]?)([^\n'\"]+)\1\s*$", text)
    checksum_values = [value.strip() for _, value in checksums]
    literal = [v for v in checksum_values if re.fullmatch(r"[0-9a-f]{64}", v)]
    skip_src = bool(re.search(r"(?m)^\s*TERMUX_PKG_SKIP_SRC_EXTRACT=true\s*$", text))
    if skip_src:
        state = "NO_EXTERNAL_SOURCE"
    elif literal:
        state = "DECLARED_SOURCE_CHECKSUM_BOUND"
    elif "SKIP_CHECKSUM" in checksum_values:
        state = "TOKEN_VAZIO_SOURCE_CHECKSUM_SKIPPED"
    else:
        state = "TOKEN_VAZIO_DECLARED_SOURCE_DIGEST"
    return {
        "source_urls_declared": urls,
        "source_checksums_declared": checksum_values,
        "source_digest_state": state,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", required=True)
    ap.add_argument("--repo-root", default=".")
    ap.add_argument("--deb-dir", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--commit", required=True)
    ap.add_argument("--arch", required=True)
    ap.add_argument("--profile", required=True)
    args = ap.parse_args()

    root = Path(args.repo_root).resolve()
    deb_dir = Path(args.deb_dir).resolve()
    rows = []
    for raw in Path(args.map).read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            continue
        output_pkg, recipe_pkg = raw.split("\t", 1)
        rows.append((output_pkg, recipe_pkg))

    deb_by_package = {}
    for deb in sorted(deb_dir.glob("*.deb")):
        pkg = deb_field(deb, "Package")
        if pkg in deb_by_package:
            raise SystemExit(f"duplicate .deb package identity: {pkg}")
        deb_by_package[pkg] = deb

    records = []
    missing = []
    for output_pkg, recipe_pkg in rows:
        recipe = root / "packages" / recipe_pkg / "build.sh"
        if not recipe.is_file():
            raise SystemExit(f"missing recipe: {recipe}")
        deb = deb_by_package.get(output_pkg)
        if deb is None:
            missing.append(output_pkg)
            continue
        recipe_text = recipe.read_text(encoding="utf-8")
        record = {
            "output_package": output_pkg,
            "package_version": deb_field(deb, "Version"),
            "package_architecture": deb_field(deb, "Architecture"),
            "deb_filename": deb.name,
            "deb_bytes": deb.stat().st_size,
            "deb_sha256": sha256_file(deb),
            "producer_recipe": f"packages/{recipe_pkg}/build.sh",
            "recipe_git_blob": git_blob(root, recipe),
            "recipe_sha256": sha256_file(recipe),
            "license_expression_declared": recipe_literal(recipe_text, "TERMUX_PKG_LICENSE"),
            "homepage_declared": recipe_literal(recipe_text, "TERMUX_PKG_HOMEPAGE"),
            **recipe_source_contract(recipe_text),
        }
        records.append(record)

    if missing:
        raise SystemExit("missing expected output .deb(s): " + ",".join(sorted(missing)))

    records.sort(key=lambda r: (r["output_package"], r["deb_filename"]))
    canonical = json.dumps(records, sort_keys=True, separators=(",", ":")).encode()
    document = {
        "schema": "rafcodephi.package-custody-manifest/v1",
        "repository": "rafaelmeloreisnovo/termux-packages",
        "source_commit": args.commit,
        "target_arch": args.arch,
        "profile": args.profile,
        "record_count": len(records),
        "records_root_sha256": hashlib.sha256(canonical).hexdigest(),
        "records": records,
        "claim_allowed": False,
        "physical_android": "TOKEN_VAZIO",
    }
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    Path(str(out) + ".sha256").write_text(f"{sha256_file(out)}  {out.name}\n", encoding="utf-8")
    print(f"RAFCODEPHI_PACKAGE_CUSTODY=PASS records={len(records)} root={document['records_root_sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
