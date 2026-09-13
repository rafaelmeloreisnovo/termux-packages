#!/usr/bin/env python3
"""Validate a repository-local RAFAELIA seven-guard work unit.

Structural validity and readiness are separate. A well-formed unit may be
intentionally BLOCKED while contradictions, uncertainty, reproduction, or
rollback gaps remain. Use ``--expect BLOCKED`` to assert that fail-closed state
without turning it into a domain/runtime PASS.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "configs" / "knowledge-work-seven-guards.v1.json"

REQUIRED_GUARDS = [
    "provenance",
    "context",
    "evidence",
    "contradiction",
    "uncertainty",
    "reproduction",
    "rollback",
]
REQUIRED_TOP = {
    "id",
    "intent",
    "repository",
    "provenance",
    "context",
    "evidence",
    "contradictions",
    "uncertainty",
    "reproduction",
    "rollback",
    "reconstruction_pointer",
    "mutation_performed",
    "claim_allowed",
}
CONTRADICTION_STATES = {"OPEN", "RESOLVED", "SUPERSEDED"}
UNCERTAINTY_STATES = {
    "TOKEN_VAZIO",
    "BLOCKED",
    "PARTIAL",
    "NOT_APPLICABLE",
    "CLOSED",
}
REPRODUCTION_STATES = {"TOKEN_VAZIO", "BLOCKED", "PASS", "FAIL", "NOT_APPLICABLE"}
ROLLBACK_STATES = {"TOKEN_VAZIO", "READY", "EXECUTED", "NOT_APPLICABLE"}
UNRESOLVED = {"TOKEN_VAZIO", "BLOCKED", "PARTIAL"}


def _nonempty(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _missing(obj: dict, keys: tuple[str, ...], prefix: str, errors: list[str]) -> None:
    for key in keys:
        if key not in obj or obj[key] in (None, ""):
            errors.append(f"{prefix}:missing_{key}")


def validate_config(cfg: dict) -> list[str]:
    errors: list[str] = []
    if cfg.get("required_guards") != REQUIRED_GUARDS:
        errors.append("config:required_guards_mismatch")
    if cfg.get("claim_allowed") is not False:
        errors.append("config:claim_allowed_must_be_false")
    if not _nonempty(cfg.get("repository")):
        errors.append("config:repository_missing")
    source = cfg.get("source_contract")
    if not isinstance(source, dict):
        errors.append("config:source_contract_missing")
    else:
        _missing(source, ("repository", "commit", "path"), "config:source_contract", errors)
    return errors


def validate_unit(unit: dict, cfg: dict) -> dict:
    errors = validate_config(cfg)
    blockers: list[str] = []

    for key in sorted(REQUIRED_TOP - set(unit)):
        errors.append(f"unit:missing_{key}")

    if not _nonempty(unit.get("id")):
        errors.append("unit:id_empty")
    if not _nonempty(unit.get("intent")):
        errors.append("unit:intent_empty")
    if unit.get("repository") != cfg.get("repository"):
        errors.append("unit:repository_mismatch")
    if unit.get("claim_allowed") is not False:
        errors.append("claim:promotion_forbidden_in_adapter")

    provenance = unit.get("provenance")
    if not isinstance(provenance, dict):
        errors.append("provenance:missing")
    else:
        fields = (
            "source_provider",
            "repository",
            "ref",
            "path",
            "object_hash",
            "observed_at",
            "authority",
        )
        _missing(provenance, fields, "provenance", errors)
        if provenance.get("repository") != cfg.get("repository"):
            errors.append("provenance:repository_mismatch")
        if provenance.get("authority") != cfg.get("role"):
            errors.append("provenance:authority_mismatch")
        for key in fields:
            value = provenance.get(key)
            if isinstance(value, str) and value.startswith("TOKEN_VAZIO"):
                blockers.append(f"provenance:{key}:TOKEN_VAZIO")

    context = unit.get("context")
    if not isinstance(context, dict):
        errors.append("context:missing")
    else:
        _missing(
            context,
            ("intent", "scope", "boundary", "observed_at", "dependencies"),
            "context",
            errors,
        )
        if "dependencies" in context and not isinstance(context["dependencies"], list):
            errors.append("context:dependencies_not_list")

    evidence = unit.get("evidence")
    if not isinstance(evidence, list):
        errors.append("evidence:not_list")
    elif not evidence:
        blockers.append("evidence:empty")
    else:
        for i, item in enumerate(evidence):
            if not isinstance(item, dict):
                errors.append(f"evidence:{i}:not_object")
                continue
            _missing(item, ("ref", "type", "scope"), f"evidence:{i}", errors)

    contradictions = unit.get("contradictions")
    if not isinstance(contradictions, list):
        errors.append("contradiction:not_list")
    else:
        for i, item in enumerate(contradictions):
            if not isinstance(item, dict):
                errors.append(f"contradiction:{i}:not_object")
                continue
            _missing(item, ("id", "state", "comparison_scope", "ref"), f"contradiction:{i}", errors)
            state = item.get("state")
            if state not in CONTRADICTION_STATES:
                errors.append(f"contradiction:{i}:invalid_state")
            if state == "OPEN":
                blockers.append(f"contradiction:{item.get('id', '?')}:OPEN")
            if state == "RESOLVED" and not _nonempty(item.get("resolution")):
                errors.append(f"contradiction:{i}:resolved_requires_resolution")

    uncertainty = unit.get("uncertainty")
    if not isinstance(uncertainty, list):
        errors.append("uncertainty:not_list")
    else:
        for i, item in enumerate(uncertainty):
            if not isinstance(item, dict):
                errors.append(f"uncertainty:{i}:not_object")
                continue
            _missing(
                item,
                ("id", "state", "evidence_needed", "falsifier", "next_probe"),
                f"uncertainty:{i}",
                errors,
            )
            state = item.get("state")
            if state not in UNCERTAINTY_STATES:
                errors.append(f"uncertainty:{i}:invalid_state")
            if state in UNRESOLVED:
                blockers.append(f"uncertainty:{item.get('id', '?')}:{state}")

    reproduction = unit.get("reproduction")
    if not isinstance(reproduction, dict):
        errors.append("reproduction:missing")
    else:
        _missing(
            reproduction,
            ("status", "procedure", "environment_ref", "input_ref", "output_ref"),
            "reproduction",
            errors,
        )
        status = reproduction.get("status")
        if status not in REPRODUCTION_STATES:
            errors.append("reproduction:invalid_status")
        elif status != "PASS":
            blockers.append(f"reproduction:{status}")

    rollback = unit.get("rollback")
    if not isinstance(rollback, dict):
        errors.append("rollback:missing")
    else:
        _missing(rollback, ("state", "predecessor", "procedure", "verification"), "rollback", errors)
        state = rollback.get("state")
        if state not in ROLLBACK_STATES:
            errors.append("rollback:invalid_state")
        if unit.get("mutation_performed") is True and state not in {"READY", "EXECUTED"}:
            blockers.append(f"rollback:{state or 'MISSING'}:mutation_requires_ready")

    if not _nonempty(unit.get("reconstruction_pointer")):
        errors.append("reconstructibility:missing_pointer")
    if not isinstance(unit.get("mutation_performed"), bool):
        errors.append("unit:mutation_performed_not_bool")

    decision = "READY_FOR_DOMAIN_REVIEW" if not errors and not blockers else "BLOCKED"
    return {
        "structural_status": "PASS" if not errors else "FAIL",
        "decision": decision,
        "errors": errors,
        "blockers": blockers,
        "claim_allowed": False,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("unit", type=Path)
    parser.add_argument(
        "--expect",
        choices=("BLOCKED", "READY_FOR_DOMAIN_REVIEW"),
        default="READY_FOR_DOMAIN_REVIEW",
        help="Expected fail-closed decision. Structural errors always fail.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    cfg = json.loads(CONFIG.read_text(encoding="utf-8"))
    unit = json.loads(args.unit.read_text(encoding="utf-8"))
    result = validate_unit(unit, cfg)
    result["expected_decision"] = args.expect
    result["expectation_met"] = not result["errors"] and result["decision"] == args.expect
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["expectation_met"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
