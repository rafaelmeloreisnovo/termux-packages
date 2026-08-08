#!/usr/bin/env python3
"""Evidence-first reality audit for core/*.c.

V1 used source heuristics as the classification itself. V2 deliberately does
not: heuristics are only risk signals. Classification comes from an explicit
registry with evidence axes. Any unregistered module is TOKEN_VAZIO.

Exit codes:
  0: registry valid and report produced
  1: --strict gate found unresolved P0/conflict
  2: registry/contract invalid or I/O failure
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Any

CLASSIFICATIONS = {
    "VERIFIED_LIMITED",
    "OBSERVED_LIMITED",
    "PROTOTYPE",
    "SIMULATED",
    "STUB",
    "TOKEN_VAZIO",
    "REFUTED",
    "OUT_OF_DOMAIN",
}
EVIDENCE_STATES = {
    "PASS",
    "FAIL",
    "BLOCKED",
    "OBSERVED",
    "OBSERVED_LIMITED",
    "TOKEN_VAZIO",
    "NOT_APPLICABLE",
}
PRIORITIES = {"P0", "P1", "P2"}
DEFAULT_AXES = (
    "code",
    "build",
    "runtime",
    "test",
    "ci",
    "device",
    "portability",
    "security",
    "provenance",
)
PROMOTION_AXES = ("code", "build", "runtime", "test", "provenance")

SIGNALS = {
    "simulation": re.compile(r"\b(simulat(?:e|ed|ion)|toy)\b", re.I),
    "stub": re.compile(r"\b(stub|TODO|FIXME|not implemented)\b", re.I),
    "mock": re.compile(r"\b(mock|dummy|fake|placeholder)\b", re.I),
    "rand": re.compile(r"\brand\s*\("),
    "io": re.compile(r"\b(fopen|open|stat|read|write|socket|connect|accept|send|recv)\s*\("),
}


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as fh:
        value = json.load(fh)
    if not isinstance(value, dict):
        raise ValueError(f"{path}: root must be an object")
    return value


def validate_registry(registry: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if registry.get("schema") != "reality_registry/2.0.0":
        errors.append("schema must be reality_registry/2.0.0")

    axes = registry.get("required_evidence_axes", list(DEFAULT_AXES))
    if not isinstance(axes, list) or set(axes) != set(DEFAULT_AXES):
        errors.append("required_evidence_axes must exactly match V2 evidence axes")
        axes = list(DEFAULT_AXES)

    promotion_axes = registry.get("promotion_axes", list(PROMOTION_AXES))
    if not isinstance(promotion_axes, list) or not set(PROMOTION_AXES).issubset(set(promotion_axes)):
        errors.append("promotion_axes must include code/build/runtime/test/provenance")
        promotion_axes = list(PROMOTION_AXES)

    modules = registry.get("modules")
    if not isinstance(modules, list):
        return errors + ["modules must be an array"]

    seen: set[str] = set()
    for idx, item in enumerate(modules):
        prefix = f"modules[{idx}]"
        if not isinstance(item, dict):
            errors.append(f"{prefix}: must be an object")
            continue
        filename = item.get("file")
        if not isinstance(filename, str) or not filename.endswith(".c"):
            errors.append(f"{prefix}.file: must be a .c filename")
            continue
        if filename in seen:
            errors.append(f"{prefix}.file: duplicate {filename}")
        seen.add(filename)

        classification = item.get("classification")
        if classification not in CLASSIFICATIONS:
            errors.append(f"{prefix}.classification: invalid {classification!r}")
        if item.get("priority") not in PRIORITIES:
            errors.append(f"{prefix}.priority: must be P0/P1/P2")
        if not isinstance(item.get("reason"), str) or not item["reason"].strip():
            errors.append(f"{prefix}.reason: required")
        if not isinstance(item.get("next_gate"), str) or not item["next_gate"].strip():
            errors.append(f"{prefix}.next_gate: required")

        evidence = item.get("evidence")
        if not isinstance(evidence, dict):
            errors.append(f"{prefix}.evidence: required object")
            continue
        if set(evidence) != set(axes):
            missing = sorted(set(axes) - set(evidence))
            extra = sorted(set(evidence) - set(axes))
            errors.append(f"{prefix}.evidence: missing={missing} extra={extra}")
        for axis, state in evidence.items():
            if state not in EVIDENCE_STATES:
                errors.append(f"{prefix}.evidence.{axis}: invalid state {state!r}")

        claim_allowed = item.get("claim_allowed")
        if not isinstance(claim_allowed, bool):
            errors.append(f"{prefix}.claim_allowed: must be boolean")
        elif claim_allowed:
            not_pass = [axis for axis in promotion_axes if evidence.get(axis) != "PASS"]
            if not_pass:
                errors.append(
                    f"{prefix}.claim_allowed: true while promotion axes not PASS: {not_pass}"
                )
            if classification not in {"VERIFIED_LIMITED"}:
                errors.append(
                    f"{prefix}.claim_allowed: true requires VERIFIED_LIMITED classification"
                )
    return errors


def scan_signals(path: Path) -> dict[str, int]:
    text = path.read_text(encoding="utf-8", errors="replace")
    return {name: len(pattern.findall(text)) for name, pattern in SIGNALS.items()}


def default_record(filename: str, axes: list[str]) -> dict[str, Any]:
    return {
        "file": filename,
        "classification": "TOKEN_VAZIO",
        "priority": "P2",
        "reason": "No explicit V2 evidence record exists for this module.",
        "evidence": {axis: "TOKEN_VAZIO" for axis in axes},
        "claim_allowed": False,
        "next_gate": "Classify source semantics, build, runtime, tests, provenance and target scope with evidence pointers.",
        "registry_state": "UNREGISTERED",
    }


def conflict_reasons(record: dict[str, Any], signals: dict[str, int]) -> list[str]:
    conflicts: list[str] = []
    classification = record["classification"]
    if classification in {"VERIFIED_LIMITED", "OBSERVED_LIMITED"}:
        if signals["simulation"] >= 2:
            conflicts.append("simulation markers conflict with observed/verified classification")
        if signals["stub"] + signals["mock"] >= 3:
            conflicts.append("stub/mock markers conflict with observed/verified classification")
    if classification == "SIMULATED" and record["evidence"].get("security") == "PASS":
        conflicts.append("simulated module cannot satisfy security axis")
    return conflicts


def build_report(core_dir: Path, registry: dict[str, Any]) -> dict[str, Any]:
    axes = list(registry.get("required_evidence_axes", DEFAULT_AXES))
    by_file = {item["file"]: item for item in registry["modules"]}
    modules: list[dict[str, Any]] = []

    for source in sorted(core_dir.glob("*.c"), key=lambda p: p.name):
        base = source.name
        raw = by_file.get(base)
        record = dict(raw) if raw is not None else default_record(base, axes)
        if raw is not None:
            record["registry_state"] = "REGISTERED"
        record["evidence"] = dict(record["evidence"])
        signals = scan_signals(source)
        record["signals"] = signals
        record["conflicts"] = conflict_reasons(record, signals)
        modules.append(record)

    classifications = Counter(item["classification"] for item in modules)
    priorities = Counter(item["priority"] for item in modules)
    registry_states = Counter(item["registry_state"] for item in modules)
    p0_unresolved = [
        item["file"]
        for item in modules
        if item["priority"] == "P0"
        and (
            item["classification"] == "TOKEN_VAZIO"
            or any(state in {"TOKEN_VAZIO", "FAIL", "BLOCKED"} for state in item["evidence"].values())
        )
    ]
    conflicts = [item["file"] for item in modules if item["conflicts"]]

    return {
        "schema": "reality_audit/2.0.0",
        "policy": "evidence-first/fail-closed",
        "source_of_classification": "explicit_registry",
        "heuristics_are_proof": False,
        "claim_allowed_default": False,
        "totals": {
            "modules": len(modules),
            "registered": registry_states.get("REGISTERED", 0),
            "unregistered": registry_states.get("UNREGISTERED", 0),
            "by_classification": dict(sorted(classifications.items())),
            "by_priority": dict(sorted(priorities.items())),
            "conflicts": len(conflicts),
            "p0_unresolved": len(p0_unresolved),
        },
        "gate": {
            "strict_pass": not p0_unresolved and not conflicts,
            "p0_unresolved_modules": p0_unresolved,
            "classification_conflicts": conflicts,
        },
        "modules": modules,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--core-dir", default="core")
    parser.add_argument("--registry", default="core/reality_registry.v2.json")
    parser.add_argument("--out", default="core/tests/fixtures/reality_audit_v2.json")
    parser.add_argument("--strict", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    core_dir = Path(args.core_dir)
    registry_path = Path(args.registry)
    out_path = Path(args.out)

    try:
        registry = load_json(registry_path)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"REGISTRY_ERROR: {exc}", file=sys.stderr)
        return 2

    errors = validate_registry(registry)
    if errors:
        print("REGISTRY_CONTRACT_FAIL", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 2

    if not core_dir.is_dir():
        print(f"CORE_DIR_ERROR: not a directory: {core_dir}", file=sys.stderr)
        return 2

    report = build_report(core_dir, registry)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    totals = report["totals"]
    gate = report["gate"]
    print(
        "REALITY_AUDIT_V2 "
        f"modules={totals['modules']} registered={totals['registered']} "
        f"token_vazio={totals['by_classification'].get('TOKEN_VAZIO', 0)} "
        f"p0_unresolved={totals['p0_unresolved']} conflicts={totals['conflicts']}"
    )
    print(f"report={out_path}")

    if args.strict and not gate["strict_pass"]:
        print("STRICT_GATE=BLOCKED", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
