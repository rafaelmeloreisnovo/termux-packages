#!/usr/bin/env python3
import copy
import importlib.util
import json
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "seven_guards", Path("scripts/validate_knowledge_work_seven_guards.py")
)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
CFG = json.loads(Path("configs/knowledge-work-seven-guards.v1.json").read_text())
BASE = json.loads(Path("examples/knowledge-work-seven-guards.example.json").read_text())

class SevenGuardTests(unittest.TestCase):
    def run_unit(self, mutate=None):
        u = copy.deepcopy(BASE)
        if mutate:
            mutate(u)
        return mod.validate_unit(u, CFG)

    def test_guard_order(self):
        self.assertEqual(CFG["required_guards"], mod.REQUIRED_GUARDS)

    def test_positive_fixture_ready(self):
        self.assertEqual(self.run_unit()["decision"], "READY_FOR_DOMAIN_REVIEW")

    def test_missing_provenance(self):
        self.assertIn("provenance:missing", self.run_unit(lambda u: u.pop("provenance"))["errors"])

    def test_invalid_provenance_hash(self):
        r = self.run_unit(lambda u: u["provenance"].__setitem__("object_hash","not-a-hash"))
        self.assertIn("provenance:invalid_object_hash", r["errors"])

    def test_github_ref_must_be_exact_commit(self):
        r = self.run_unit(lambda u: u["provenance"].__setitem__("ref","main"))
        self.assertIn("provenance:github_ref_not_exact_commit", r["errors"])

    def test_missing_context(self):
        self.assertIn("context:missing", self.run_unit(lambda u: u.pop("context"))["errors"])

    def test_empty_evidence_blocks(self):
        self.assertIn("evidence:empty", self.run_unit(lambda u: u.__setitem__("evidence", []))["blockers"])

    def test_invalid_evidence_type(self):
        def m(u): u["evidence"]=[{"ref":"trust-me","type":"WHATEVER","scope":"all"}]
        self.assertIn("evidence:0:invalid_type", self.run_unit(m)["errors"])

    def test_open_contradiction_blocks(self):
        def m(u): u["contradictions"]=[{"id":"C1","state":"OPEN","comparison_scope":"same recipe/artifact","ref":"fixture:C1"}]
        self.assertTrue(any("OPEN" in x for x in self.run_unit(m)["blockers"]))

    def test_invalid_contradiction_state_rejected(self):
        def m(u): u["contradictions"]=[{"id":"C1","state":"IGNORED","comparison_scope":"same recipe/artifact","ref":"fixture:C1"}]
        self.assertIn("contradiction:0:invalid_state", self.run_unit(m)["errors"])

    def test_uncertainty_blocks(self):
        def m(u): u["uncertainty"]=[{"id":"U1","state":"TOKEN_VAZIO","evidence_needed":"artifact digest","falsifier":"receipt","next_probe":"build bounded package"}]
        self.assertTrue(any("TOKEN_VAZIO" in x for x in self.run_unit(m)["blockers"]))

    def test_invalid_uncertainty_state_rejected(self):
        def m(u): u["uncertainty"]=[{"id":"U1","state":"MAGIC","evidence_needed":"artifact digest","falsifier":"receipt","next_probe":"build bounded package"}]
        self.assertIn("uncertainty:0:invalid_state", self.run_unit(m)["errors"])

    def test_reproduction_required(self):
        def m(u): u["reproduction"]["status"]="TOKEN_VAZIO"
        self.assertTrue(any(x.startswith("reproduction:") for x in self.run_unit(m)["blockers"]))

    def test_reproduction_pass_requires_reproduction_evidence(self):
        def m(u): u["evidence"]=[{"ref":"data:1","type":"DATA","scope":"bounded"}]
        self.assertIn("reproduction:pass_without_reproduction_evidence", self.run_unit(m)["blockers"])

    def test_invalid_rollback_state_rejected(self):
        def m(u): u["rollback"]["state"]="MAGIC"
        self.assertIn("rollback:invalid_state", self.run_unit(m)["errors"])

    def test_mutation_requires_rollback(self):
        def m(u):
            u["mutation_performed"]=True
            u["rollback"]["state"]="NOT_APPLICABLE"
        self.assertTrue(any("mutation_requires_ready" in x for x in self.run_unit(m)["blockers"]))

    def test_reconstruction_pointer_must_be_canonical(self):
        r = self.run_unit(lambda u: u.__setitem__("reconstruction_pointer","garbage"))
        self.assertIn("reconstructibility:pointer_not_canonical", r["errors"])

    def test_claim_promotion_rejected(self):
        r = self.run_unit(lambda u: u.__setitem__("claim_allowed", True))
        self.assertIn("claim:promotion_forbidden_in_adapter", r["errors"])

if __name__ == "__main__":
    unittest.main(verbosity=2)
