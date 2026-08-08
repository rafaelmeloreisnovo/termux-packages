#!/usr/bin/env python3
"""Conservative source-level dependency closure for RAFCODEPHI.

This is a preflight, not a Bash evaluator. It reads literal TERMUX_PKG_DEPENDS and
TERMUX_PKG_BUILD_DEPENDS assignments, follows first alternatives, and refuses to
promote a closure if referenced package recipes are missing. Dynamic shell
expansions remain explicit TOKEN_VAZIO and are reported, not guessed.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import deque
from pathlib import Path
from typing import Iterable

SCHEMA = "rafcodephi_dependency_closure/1.0.0"
ROOTS = ("packages", "root-packages", "x11-packages", "disabled-packages")
FIELDS = ("TERMUX_PKG_DEPENDS", "TERMUX_PKG_BUILD_DEPENDS")
ASSIGN_RE = re.compile(r"^(TERMUX_PKG_(?:BUILD_)?DEPENDS)\s*(\+?=)\s*(.*)$")
DYNAMIC_RE = re.compile(r"\$(?:\{|\(|[A-Za-z_])|`|\$\(")


def logical_lines(text: str) -> list[str]:
    out: list[str] = []
    buf = ""
    for raw in text.splitlines():
        line = raw.rstrip()
        if buf:
            buf += line
        else:
            buf = line
        if buf.endswith("\\"):
            buf = buf[:-1]
            continue
        out.append(buf)
        buf = ""
    if buf:
        out.append(buf)
    return out


def unquote(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        return value[1:-1]
    return value


def parse_literal_deps(path: Path) -> tuple[list[str], list[str]]:
    values = {field: "" for field in FIELDS}
    dynamic: list[str] = []
    try:
        text = path.read_text(encoding="utf-8", errors="strict")
    except OSError as exc:
        raise RuntimeError(f"cannot read {path}: {exc}") from exc

    for line in logical_lines(text):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        m = ASSIGN_RE.match(stripped)
        if not m:
            continue
        field, op, raw_value = m.groups()
        value = unquote(raw_value.strip())
        if DYNAMIC_RE.search(value):
            dynamic.append(f"{field}:{value}")
            continue
        if op == "+=":
            values[field] = ",".join(x for x in (values[field], value) if x)
        else:
            values[field] = value

    deps: list[str] = []
    for field in FIELDS:
        for chunk in values[field].split(","):
            chunk = chunk.strip()
            if not chunk:
                continue
            # This is deliberately a first-alternative source projection.
            first = chunk.split("|", 1)[0].strip()
            # Strip version constraint and common architecture suffix notation.
            name = re.split(r"\s|\(", first, maxsplit=1)[0].strip()
            name = name.split(":", 1)[0].strip()
            if name:
                deps.append(name)
    return deps, dynamic


def inventory(repo: Path) -> tuple[dict[str, Path], list[str]]:
    mapping: dict[str, Path] = {}
    duplicates: list[str] = []
    for root in ROOTS:
        base = repo / root
        if not base.is_dir():
            continue
        for build in base.glob("*/build.sh"):
            name = build.parent.name
            if name in mapping and mapping[name] != build:
                duplicates.append(name)
            else:
                mapping[name] = build
        for sub in base.glob("*/*.subpackage.sh"):
            name = sub.name[: -len(".subpackage.sh")]
            if name in mapping and mapping[name] != sub:
                duplicates.append(name)
            else:
                mapping[name] = sub
    return mapping, sorted(set(duplicates))


def build_closure(repo: Path, root_package: str) -> dict:
    inv, duplicates = inventory(repo)
    if root_package not in inv:
        raise RuntimeError(f"root package not found: {root_package}")
    if duplicates:
        raise RuntimeError(f"duplicate package names block deterministic closure: {duplicates}")

    queue: deque[str] = deque([root_package])
    seen: set[str] = set()
    edges: list[tuple[str, str]] = []
    unresolved: set[str] = set()
    dynamic: dict[str, list[str]] = {}

    while queue:
        package = queue.popleft()
        if package in seen:
            continue
        seen.add(package)
        path = inv.get(package)
        if path is None:
            unresolved.add(package)
            continue
        deps, dyn = parse_literal_deps(path)
        if dyn:
            dynamic[package] = dyn
        for dep in deps:
            edges.append((package, dep))
            if dep not in inv:
                unresolved.add(dep)
            elif dep not in seen:
                queue.append(dep)

    closure = sorted(name for name in seen if name in inv)
    result = {
        "schema": SCHEMA,
        "state": "OBSERVED_LIMITED",
        "claim_allowed": False,
        "root_package": root_package,
        "projection": "literal_first_alternative_TERMUX_PKG_DEPENDS_and_BUILD_DEPENDS",
        "custom_prefix_rule": "all target dependencies must be built for the same TERMUX_APP_PACKAGE/prefix",
        "closure_count": len(closure),
        "closure": closure,
        "edge_count": len(edges),
        "edges": [{"from": a, "to": b} for a, b in sorted(edges)],
        "unresolved_count": len(unresolved),
        "unresolved": sorted(unresolved),
        "dynamic_dependency_fields": dynamic,
        "dynamic_dependency_package_count": len(dynamic),
        "complete_for_literal_projection": len(unresolved) == 0 and len(dynamic) == 0,
        "artifact_build_rule": "do_not_use_-I_with_custom_TERMUX_APP_PACKAGE",
        "next_gate": "run the existing build-package.sh without -I for the requested target architecture and validate each resulting .deb",
    }
    return result


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("package")
    ap.add_argument("--repo", type=Path, default=Path("."))
    ap.add_argument("--out", type=Path)
    ap.add_argument("--strict", action="store_true")
    args = ap.parse_args()
    try:
        result = build_closure(args.repo, args.package)
    except RuntimeError as exc:
        print(f"CLOSURE_GATE=BLOCKED reason={exc}", file=sys.stderr)
        return 2

    text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.out:
        args.out.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)

    print(
        f"CLOSURE_GATE=OBSERVED_LIMITED package={args.package} "
        f"closure={result['closure_count']} unresolved={result['unresolved_count']} "
        f"dynamic={result['dynamic_dependency_package_count']}",
        file=sys.stderr,
    )
    if args.strict and not result["complete_for_literal_projection"]:
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
