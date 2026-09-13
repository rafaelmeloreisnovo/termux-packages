#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path

CONFIG = Path("configs/knowledge-work-seven-guards.v1.json")
REQUIRED_GUARDS = [
    "provenance", "context", "evidence", "contradiction",
    "uncertainty", "reproduction", "rollback"
]
UNRESOLVED = {"TOKEN_VAZIO", "BLOCKED", "PARTIAL"}
HEX_DIGEST = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64}|sha1:[0-9a-f]{40}|sha256:[0-9a-f]{64})$")
GITHUB_COMMIT = re.compile(r"^[0-9a-f]{40}$")
ISO_OBSERVED = re.compile(r"^\d{4}-\d{2}-\d{2}(?:T\d{2}:\d{2}(?::\d{2})?(?:Z|[+-]\d{2}:?\d{2})?)?$")

def _missing(obj, keys, prefix, errors):
    for key in keys:
        if key not in obj or obj[key] in (None, ""):
            errors.append(f"{prefix}:missing_{key}")

def _token_vazio(value):
    return isinstance(value, str) and value.startswith("TOKEN_VAZIO")

def validate_unit(unit, cfg):
    errors, blockers = [], []
    states = cfg.get("allowed_states", {})
    allowed_evidence = set(cfg.get("allowed_evidence_types", []))
    reproduction_evidence = set(cfg.get("reproduction_evidence_types", []))

    if cfg.get("required_guards") != REQUIRED_GUARDS:
        errors.append("config:required_guards_mismatch")
    if cfg.get("claim_allowed") is not False:
        errors.append("config:claim_allowed_must_be_false")
    if unit.get("repository") != cfg.get("repository"):
        errors.append("unit:repository_mismatch")
    if unit.get("claim_allowed") is not False:
        errors.append("claim:promotion_forbidden_in_adapter")

    prov = unit.get("provenance")
    if not isinstance(prov, dict):
        errors.append("provenance:missing")
    else:
        fields = ("source_provider","repository","ref","path","object_hash","observed_at","authority")
        _missing(prov, fields, "provenance", errors)
        for key in fields:
            if _token_vazio(prov.get(key)):
                blockers.append(f"provenance:{key}:TOKEN_VAZIO")
        if prov.get("object_hash") and not HEX_DIGEST.fullmatch(str(prov["object_hash"])):
            errors.append("provenance:invalid_object_hash")
        if prov.get("source_provider") == "GitHub":
            if not GITHUB_COMMIT.fullmatch(str(prov.get("ref",""))):
                errors.append("provenance:github_ref_not_exact_commit")
            if "/" not in str(prov.get("repository","")):
                errors.append("provenance:github_repository_invalid")
        if prov.get("observed_at") and not ISO_OBSERVED.fullmatch(str(prov["observed_at"])):
            errors.append("provenance:observed_at_invalid")

    ctx = unit.get("context")
    if not isinstance(ctx, dict):
        errors.append("context:missing")
    else:
        _missing(ctx, ("intent","scope","boundary","observed_at","dependencies"), "context", errors)
        if "dependencies" in ctx and not isinstance(ctx["dependencies"], list):
            errors.append("context:dependencies_not_list")
        elif isinstance(ctx.get("dependencies"), list):
            for i, dep in enumerate(ctx["dependencies"]):
                if not isinstance(dep, str) or not dep:
                    errors.append(f"context:dependency_{i}_invalid")
                elif _token_vazio(dep):
                    blockers.append(f"context:dependency_{i}:TOKEN_VAZIO")
        if ctx.get("observed_at") and not ISO_OBSERVED.fullmatch(str(ctx["observed_at"])):
            errors.append("context:observed_at_invalid")

    evidence = unit.get("evidence")
    evidence_types = set()
    if not isinstance(evidence, list):
        errors.append("evidence:not_list")
    elif not evidence:
        blockers.append("evidence:empty")
    else:
        for i, item in enumerate(evidence):
            if not isinstance(item, dict):
                errors.append(f"evidence:{i}:not_object")
                continue
            _missing(item, ("ref","type","scope"), f"evidence:{i}", errors)
            etype = item.get("type")
            if etype not in allowed_evidence:
                errors.append(f"evidence:{i}:invalid_type")
            else:
                evidence_types.add(etype)
            if _token_vazio(item.get("ref")) or _token_vazio(item.get("scope")):
                blockers.append(f"evidence:{i}:TOKEN_VAZIO")

    contradictions = unit.get("contradictions")
    if not isinstance(contradictions, list):
        errors.append("contradiction:not_list")
    else:
        allowed = set(states.get("contradiction", []))
        for i, item in enumerate(contradictions):
            if not isinstance(item, dict):
                errors.append(f"contradiction:{i}:not_object")
                continue
            _missing(item, ("id","state","comparison_scope","ref"), f"contradiction:{i}", errors)
            state = item.get("state")
            if state not in allowed:
                errors.append(f"contradiction:{i}:invalid_state")
            elif state == "OPEN":
                blockers.append(f"contradiction:{item.get('id','?')}:OPEN")

    uncertainty = unit.get("uncertainty")
    if not isinstance(uncertainty, list):
        errors.append("uncertainty:not_list")
    else:
        allowed = set(states.get("uncertainty", []))
        for i, item in enumerate(uncertainty):
            if not isinstance(item, dict):
                errors.append(f"uncertainty:{i}:not_object")
                continue
            _missing(item, ("id","state","evidence_needed","falsifier","next_probe"), f"uncertainty:{i}", errors)
            state = item.get("state")
            if state not in allowed:
                errors.append(f"uncertainty:{i}:invalid_state")
            elif state in UNRESOLVED:
                blockers.append(f"uncertainty:{item.get('id','?')}:{state}")

    rep = unit.get("reproduction")
    if not isinstance(rep, dict):
        errors.append("reproduction:missing")
    else:
        _missing(rep, ("status","procedure","environment_ref","input_ref","output_ref"), "reproduction", errors)
        status = rep.get("status")
        if status not in set(states.get("reproduction", [])):
            errors.append("reproduction:invalid_status")
        elif status != "PASS":
            blockers.append(f"reproduction:{status}")
        elif not (evidence_types & reproduction_evidence):
            blockers.append("reproduction:pass_without_reproduction_evidence")
        for key in ("procedure","environment_ref","input_ref","output_ref"):
            if _token_vazio(rep.get(key)):
                blockers.append(f"reproduction:{key}:TOKEN_VAZIO")

    rb = unit.get("rollback")
    if not isinstance(rb, dict):
        errors.append("rollback:missing")
    else:
        _missing(rb, ("state","predecessor","procedure","verification"), "rollback", errors)
        state = rb.get("state")
        if state not in set(states.get("rollback", [])):
            errors.append("rollback:invalid_state")
        if unit.get("mutation_performed") is True and state not in {"READY","EXECUTED"}:
            blockers.append(f"rollback:{state or 'MISSING'}:mutation_requires_ready")
        for key in ("predecessor","procedure","verification"):
            if _token_vazio(rb.get(key)):
                blockers.append(f"rollback:{key}:TOKEN_VAZIO")

    expected_pointer = None
    source_contract = cfg.get("source_contract", {})
    if all(source_contract.get(k) for k in ("repository","commit","path")):
        expected_pointer = f"{source_contract['repository']}@{source_contract['commit']}:{source_contract['path']}"
    pointer = unit.get("reconstruction_pointer")
    if not pointer:
        errors.append("reconstructibility:missing_pointer")
    elif expected_pointer and pointer != expected_pointer:
        errors.append("reconstructibility:pointer_not_canonical")

    if not isinstance(unit.get("mutation_performed"), bool):
        errors.append("unit:mutation_performed_not_bool")

    decision = "READY_FOR_DOMAIN_REVIEW" if not errors and not blockers else "BLOCKED"
    return {"decision": decision, "errors": errors, "blockers": blockers, "claim_allowed": False}

def main():
    if len(sys.argv) != 2:
        print("usage: validate_knowledge_work_seven_guards.py UNIT.json", file=sys.stderr)
        return 2
    cfg = json.loads(CONFIG.read_text(encoding="utf-8"))
    unit = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    result = validate_unit(unit, cfg)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["decision"] == "READY_FOR_DOMAIN_REVIEW" else 1

if __name__ == "__main__":
    raise SystemExit(main())
