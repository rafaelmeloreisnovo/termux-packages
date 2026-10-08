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



def dynamic_static_producer(root: Path, output_pkg: str, debs: dict[str, Path]) -> str | None:
    """Resolve a generated -static subpackage only with verified control fields.

    Termux creates these subpackage descriptors in TERMUX_PKG_TMPDIR, outside
    packages/*/*.subpackage.sh; suffix similarity alone is NOT sufficient.
    """
    if not output_pkg.endswith("-static"):
        return None
    parent = output_pkg[:-len("-static")]
    recipe = root / "packages" / parent / "build.sh"
    rule = root / "scripts/build/termux_create_debian_subpackages.sh"
    if parent not in debs or not recipe.is_file() or not rule.is_file():
        return None
    rule_source = rule.read_text(encoding="utf-8")
    required = (
        '${TERMUX_PKG_NAME}-static.subpackage.sh',
        'TERMUX_PKG_NO_STATICSPLIT',
        'Static libraries for ${TERMUX_PKG_NAME}',
    )
    if any(token not in rule_source for token in required):
        raise SystemExit("static subpackage generator contract changed; provenance BLOCKED")
    recipe_source = recipe.read_text(encoding="utf-8")
    if any(
        line.split("=", 1)[1].split("#", 1)[0].strip().strip("'\\\"") == "true"
        for line in recipe_source.splitlines()
        if line.lstrip().startswith("TERMUX_PKG_NO_STATICSPLIT=")
    ):
        return None

    pkg_deb, parent_deb = debs[output_pkg], debs[parent]
    pkg_version = deb_field(pkg_deb, "Version")
    parent_version = deb_field(parent_deb, "Version")
    depends = deb_field(pkg_deb, "Depends")
    description = deb_field(pkg_deb, "Description").splitlines()[0]
    parent_dependency = re.compile(
        r"(?:^|,\s*)" + re.escape(parent) + r"\s*\(=\s*" +
        re.escape(parent_version) + r"\)(?:\s*,|\s*$)"
    )
    if (
        pkg_version != parent_version
        or description != f"Static libraries for {parent}"
        or not parent_dependency.search(depends)
    ):
        return None
    return parent


def main() -> int:
    ap = argparse.ArgumentParser()
    source_group = ap.add_mutually_exclusive_group(required=True)
    source_group.add_argument("--map")
    source_group.add_argument("--auto-map", action="store_true")
    ap.add_argument("--repo-root", default=".")
    ap.add_argument("--deb-dir", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--commit", required=True)
    ap.add_argument("--arch", required=True)
    ap.add_argument("--profile", required=True)
    args = ap.parse_args()

    root = Path(args.repo_root).resolve()
    deb_dir = Path(args.deb_dir).resolve()
    deb_by_package = {}
    for deb in sorted(deb_dir.glob("*.deb")):
        pkg = deb_field(deb, "Package")
        if pkg in deb_by_package:
            raise SystemExit(f"duplicate .deb package identity: {pkg}")
        deb_by_package[pkg] = deb

    rows = []
    if args.map:
        for raw in Path(args.map).read_text(encoding="utf-8").splitlines():
            if not raw.strip():
                continue
            output_pkg, recipe_pkg = raw.split("\t", 1)
            rows.append((output_pkg, recipe_pkg))
    else:
        packages_root = root / "packages"
        for output_pkg in sorted(deb_by_package):
            direct = packages_root / output_pkg / "build.sh"
            candidates = []
            if direct.is_file():
                candidates.append(direct.parent.name)
            for subpackage in packages_root.glob(f"*/{output_pkg}.subpackage.sh"):
                candidates.append(subpackage.parent.name)
            dynamic_parent = dynamic_static_producer(root, output_pkg, deb_by_package)
            if dynamic_parent is not None:
                candidates.append(dynamic_parent)
            candidates = sorted(set(candidates))
            if len(candidates) != 1:
                raise SystemExit(
                    f"cannot resolve unique producing recipe for {output_pkg}: {candidates}"
                )
            rows.append((output_pkg, candidates[0]))

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
            "recipe_resolution": (
                "AUTO_DERIVED_STATIC_SPLIT_VERIFIED"
                if output_pkg.endswith("-static")
                and recipe_pkg == output_pkg[:-len("-static")]
                and args.auto_map
                else ("AUTO_DISCOVERED" if args.auto_map else "EXPLICIT_MAP")
            ),
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
