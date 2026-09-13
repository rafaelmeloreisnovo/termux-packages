#!/usr/bin/env python3
import json
import sys
from pathlib import Path

CONFIG = Path("configs/knowledge-work-seven-guards.v1.json")
REQUIRED_GUARDS = [
    "provenance", "context", "evidence", "contradiction",
    "uncertainty", "reproduction", "rollback"
]
UNRESOLVED = {"TOKEN_VAZIO", "BLOCKED", "PARTIAL"}

def _missing(obj, keys, prefix, errors):
    for key in keys:
        if key not in obj or obj[key] in (None, ""):
            errors.append(f"{prefix}:missing_{key}")

def validate_unit(unit, cfg):
    errors, blockers = [], []
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
            value = prov.get(key)
            if isinstance(value, str) and value.startswith("TOKEN_VAZIO"):
                blockers.append(f"provenance:{key}:TOKEN_VAZIO")

    ctx = unit.get("context")
    if not isinstance(ctx, dict):
        errors.append("context:missing")
    else:
        _missing(ctx, ("intent","scope","boundary","observed_at","dependencies"), "context", errors)
        if "dependencies" in ctx and not isinstance(ctx["dependencies"], list):
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
            _missing(item, ("ref","type","scope"), f"evidence:{i}", errors)

    contradictions = unit.get("contradictions")
    if not isinstance(contradictions, list):
        errors.append("contradiction:not_list")
    else:
        for i, item in enumerate(contradictions):
            if not isinstance(item, dict):
                errors.append(f"contradiction:{i}:not_object")
                continue
            _missing(item, ("id","state","comparison_scope","ref"), f"contradiction:{i}", errors)
            if item.get("state") == "OPEN":
                blockers.append(f"contradiction:{item.get('id','?')}:OPEN")

    uncertainty = unit.get("uncertainty")
    if not isinstance(uncertainty, list):
        errors.append("uncertainty:not_list")
    else:
        for i, item in enumerate(uncertainty):
            if not isinstance(item, dict):
                errors.append(f"uncertainty:{i}:not_object")
                continue
            _missing(item, ("id","state","evidence_needed","falsifier","next_probe"), f"uncertainty:{i}", errors)
            if item.get("state") in UNRESOLVED:
                blockers.append(f"uncertainty:{item.get('id','?')}:{item.get('state')}")

    rep = unit.get("reproduction")
    if not isinstance(rep, dict):
        errors.append("reproduction:missing")
    else:
        _missing(rep, ("status","procedure","environment_ref","input_ref","output_ref"), "reproduction", errors)
        if rep.get("status") != "PASS":
            blockers.append(f"reproduction:{rep.get('status','MISSING')}")

    rb = unit.get("rollback")
    if not isinstance(rb, dict):
        errors.append("rollback:missing")
    else:
        _missing(rb, ("state","predecessor","procedure","verification"), "rollback", errors)
        if unit.get("mutation_performed") is True and rb.get("state") not in {"READY","EXECUTED"}:
            blockers.append(f"rollback:{rb.get('state','MISSING')}:mutation_requires_ready")

    if not unit.get("reconstruction_pointer"):
        errors.append("reconstructibility:missing_pointer")
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
