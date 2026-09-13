#!/usr/bin/env python3
import copy
import importlib.util
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "seven_guards", ROOT / "scripts" / "validate_knowledge_work_seven_guards.py"
)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
CFG = json.loads((ROOT / "configs" / "knowledge-work-seven-guards.v1.json").read_text())
BASE = json.loads((ROOT / "examples" / "knowledge-work-seven-guards.example.json").read_text())
HANDOFF = json.loads(
    (ROOT / "examples" / "knowledge-work-seven-guards.handoff-20260913.json").read_text()
)


class SevenGuardTests(unittest.TestCase):
    def run_unit(self, mutate=None):
        unit = copy.deepcopy(BASE)
        if mutate:
            mutate(unit)
        return mod.validate_unit(unit, CFG)

    def test_guard_order(self):
        self.assertEqual(CFG["required_guards"], mod.REQUIRED_GUARDS)

    def test_positive_fixture_ready(self):
        result = self.run_unit()
        self.assertEqual(result["structural_status"], "PASS")
        self.assertEqual(result["decision"], "READY_FOR_DOMAIN_REVIEW")

    def test_real_handoff_is_structurally_valid_but_blocked(self):
        result = mod.validate_unit(HANDOFF, CFG)
        self.assertEqual(result["errors"], [])
        self.assertEqual(result["decision"], "BLOCKED")
        self.assertIn("contradiction:C-PKG-HANDOFF-COMMIT-001:OPEN", result["blockers"])
        self.assertIn("uncertainty:U-PKG-SOURCE-RECIPE-ARTIFACT-001:TOKEN_VAZIO", result["blockers"])

    def test_missing_top_level_id(self):
        result = self.run_unit(lambda unit: unit.pop("id"))
        self.assertIn("unit:missing_id", result["errors"])

    def test_missing_provenance(self):
        result = self.run_unit(lambda unit: unit.pop("provenance"))
        self.assertIn("provenance:missing", result["errors"])

    def test_provenance_authority_must_match_repository_role(self):
        result = self.run_unit(lambda unit: unit["provenance"].__setitem__("authority", "OTHER"))
        self.assertIn("provenance:authority_mismatch", result["errors"])

    def test_missing_context(self):
        result = self.run_unit(lambda unit: unit.pop("context"))
        self.assertIn("context:missing", result["errors"])

    def test_empty_evidence_blocks(self):
        result = self.run_unit(lambda unit: unit.__setitem__("evidence", []))
        self.assertIn("evidence:empty", result["blockers"])

    def test_open_contradiction_blocks(self):
        def mutate(unit):
            unit["contradictions"] = [
                {
                    "id": "C1",
                    "state": "OPEN",
                    "comparison_scope": "same recipe/artifact",
                    "ref": "fixture:C1",
                }
            ]

        self.assertIn("contradiction:C1:OPEN", self.run_unit(mutate)["blockers"])

    def test_resolved_contradiction_requires_resolution(self):
        def mutate(unit):
            unit["contradictions"] = [
                {
                    "id": "C1",
                    "state": "RESOLVED",
                    "comparison_scope": "same recipe/artifact",
                    "ref": "fixture:C1",
                }
            ]

        self.assertIn("contradiction:0:resolved_requires_resolution", self.run_unit(mutate)["errors"])

    def test_invalid_contradiction_state_fails_structure(self):
        def mutate(unit):
            unit["contradictions"] = [
                {
                    "id": "C1",
                    "state": "MAYBE",
                    "comparison_scope": "same recipe/artifact",
                    "ref": "fixture:C1",
                }
            ]

        self.assertIn("contradiction:0:invalid_state", self.run_unit(mutate)["errors"])

    def test_uncertainty_blocks(self):
        def mutate(unit):
            unit["uncertainty"] = [
                {
                    "id": "U1",
                    "state": "TOKEN_VAZIO",
                    "evidence_needed": "artifact digest",
                    "falsifier": "receipt",
                    "next_probe": "build bounded package",
                }
            ]

        self.assertIn("uncertainty:U1:TOKEN_VAZIO", self.run_unit(mutate)["blockers"])

    def test_invalid_uncertainty_state_fails_structure(self):
        def mutate(unit):
            unit["uncertainty"] = [
                {
                    "id": "U1",
                    "state": "UNKNOWNISH",
                    "evidence_needed": "artifact digest",
                    "falsifier": "receipt",
                    "next_probe": "build bounded package",
                }
            ]

        self.assertIn("uncertainty:0:invalid_state", self.run_unit(mutate)["errors"])

    def test_reproduction_required(self):
        def mutate(unit):
            unit["reproduction"]["status"] = "TOKEN_VAZIO"

        self.assertIn("reproduction:TOKEN_VAZIO", self.run_unit(mutate)["blockers"])

    def test_invalid_reproduction_state_fails_structure(self):
        def mutate(unit):
            unit["reproduction"]["status"] = "LIKELY"

        self.assertIn("reproduction:invalid_status", self.run_unit(mutate)["errors"])

    def test_mutation_requires_rollback(self):
        def mutate(unit):
            unit["mutation_performed"] = True
            unit["rollback"]["state"] = "NOT_APPLICABLE"

        self.assertIn(
            "rollback:NOT_APPLICABLE:mutation_requires_ready",
            self.run_unit(mutate)["blockers"],
        )

    def test_invalid_rollback_state_fails_structure(self):
        result = self.run_unit(lambda unit: unit["rollback"].__setitem__("state", "POSSIBLE"))
        self.assertIn("rollback:invalid_state", result["errors"])

    def test_claim_promotion_rejected(self):
        result = self.run_unit(lambda unit: unit.__setitem__("claim_allowed", True))
        self.assertIn("claim:promotion_forbidden_in_adapter", result["errors"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
