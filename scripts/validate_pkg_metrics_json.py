#!/usr/bin/env python3
"""Strict structural validator for pkg_metrics/1.0.0.

This complements the compact C validator. It is intentionally duplicate-key
aware and scope-aware: required top-level fields cannot be satisfied by nested
objects, and provenance fields must live inside the provenance object.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any

SCHEMA = "pkg_metrics/1.0.0"

TOP_U32 = (
    "node_count",
    "edge_count",
    "depends_edges",
    "build_dep_edges",
    "unresolved_count",
    "cycle_count",
    "max_depth",
    "topo_ordered",
)
TOP_U64 = ("inventory_latency_us", "dag_latency_us", "total_latency_us")
TOP_FLOAT_01 = ("coherence_phi", "graph_completeness", "graph_acyclicity")
TOP_FLOAT_OTHER = ("avg_deps_per_pkg",)
PROVENANCE_STR = (
    "schema_version",
    "git_commit",
    "build_timestamp_utc",
    "cflags_fingerprint",
    "toolchain_id",
    "producer_name",
    "host_uname",
)
PROVENANCE_INT = ("run_timestamp_unix_ms",)


class DuplicateKeyError(ValueError):
    pass


def no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for key, value in pairs:
        if key in out:
            raise DuplicateKeyError(f"duplicate key: {key}")
        out[key] = value
    return out


def reject_constant(value: str) -> Any:
    raise ValueError(f"non-finite/non-standard JSON number: {value}")


def load_strict(path: Path) -> Any:
    return json.loads(
        path.read_text(encoding="utf-8"),
        object_pairs_hook=no_duplicates,
        parse_constant=reject_constant,
    )


def is_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def is_number(value: Any) -> bool:
    return (isinstance(value, (int, float)) and not isinstance(value, bool)
            and math.isfinite(float(value)))


def contains_token_vazio(value: Any) -> bool:
    if isinstance(value, str):
        return "TOKEN_VAZIO" in value
    if isinstance(value, dict):
        return any(contains_token_vazio(k) or contains_token_vazio(v)
                   for k, v in value.items())
    if isinstance(value, list):
        return any(contains_token_vazio(v) for v in value)
    return False


def validate(data: Any) -> list[str]:
    errors: list[str] = []
    if not isinstance(data, dict):
        return ["top-level JSON must be an object"]

    if data.get("schema") != SCHEMA:
        errors.append(f"schema must equal {SCHEMA!r}")
    if data.get("status") != "REAL":
        errors.append("status must equal 'REAL'")

    provenance = data.get("provenance")
    if not isinstance(provenance, dict):
        errors.append("provenance must be an object")
        provenance = {}

    for key in TOP_U32:
        value = data.get(key)
        if not is_int(value) or value < 0 or value > 0xFFFFFFFF:
            errors.append(f"{key} must be uint32 at top level")

    for key in TOP_U64:
        value = data.get(key)
        if not is_int(value) or value < 0 or value > 0xFFFFFFFFFFFFFFFF:
            errors.append(f"{key} must be uint64 at top level")

    for key in TOP_FLOAT_01:
        value = data.get(key)
        if not is_number(value) or not (0.0 <= float(value) <= 1.0):
            errors.append(f"{key} must be finite number in [0,1] at top level")

    for key in TOP_FLOAT_OTHER:
        value = data.get(key)
        if not is_number(value) or not (0.0 <= float(value) <= 10000.0):
            errors.append(f"{key} must be finite number in [0,10000] at top level")

    for key in PROVENANCE_STR:
        value = provenance.get(key)
        if not isinstance(value, str) or not value.strip():
            errors.append(f"provenance.{key} must be a non-empty string")

    for key in PROVENANCE_INT:
        value = provenance.get(key)
        if not is_int(value) or value < 0:
            errors.append(f"provenance.{key} must be a non-negative integer")

    if provenance.get("schema_version") != SCHEMA:
        errors.append(f"provenance.schema_version must equal {SCHEMA!r}")

    if contains_token_vazio(data):
        errors.append("TOKEN_VAZIO is forbidden in promoted pkg_metrics artifact")

    # Cross-field invariants are evaluated only when operands are valid numbers.
    if all(is_int(data.get(k)) for k in ("edge_count", "depends_edges", "build_dep_edges")):
        if data["edge_count"] != data["depends_edges"] + data["build_dep_edges"]:
            errors.append("edge_count != depends_edges + build_dep_edges")

    if all(is_int(data.get(k)) for k in ("cycle_count", "topo_ordered", "node_count")):
        if data["node_count"] <= 0:
            errors.append("node_count must be > 0")
        if data["cycle_count"] == 0 and data["topo_ordered"] != data["node_count"]:
            errors.append("cycle_count=0 requires topo_ordered == node_count")

    if all(is_number(data.get(k)) for k in
           ("coherence_phi", "graph_completeness", "graph_acyclicity")):
        expected = float(data["graph_completeness"]) * float(data["graph_acyclicity"])
        if abs(float(data["coherence_phi"]) - expected) > 0.0001:
            errors.append("coherence_phi != graph_completeness * graph_acyclicity")

    if is_int(data.get("cycle_count")) and is_number(data.get("graph_acyclicity")):
        if data["cycle_count"] > 0 and float(data["graph_acyclicity"]) >= 1.0:
            errors.append("cycle_count > 0 requires graph_acyclicity < 1")

    if all(is_int(data.get(k)) for k in ("edge_count", "unresolved_count")) and is_number(data.get("graph_completeness")):
        if data["edge_count"] > 0:
            expected = 1.0 - (data["unresolved_count"] / data["edge_count"])
            if abs(float(data["graph_completeness"]) - expected) > 0.0001:
                errors.append("graph_completeness != 1 - unresolved_count/edge_count")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("json_file", type=Path)
    args = parser.parse_args()

    try:
        data = load_strict(args.json_file)
    except (OSError, UnicodeError, json.JSONDecodeError, DuplicateKeyError, ValueError) as exc:
        print(f"STRICT_JSON_GATE=BLOCKED: {exc}", file=sys.stderr)
        return 1

    errors = validate(data)
    if errors:
        print("STRICT_JSON_GATE=BLOCKED", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("STRICT_JSON_GATE=PASS")
    print(f"schema={SCHEMA}")
    print("duplicate_keys=REJECTED")
    print("scope_enforcement=PASS")
    print("claim_scope=pkg_metrics_only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
