#!/usr/bin/env python3
"""Security regression checks on the new external-event execution boundary."""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]


class WorkflowBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.text = (ROOT / ".github/workflows/rafcodephi-webhook-intake.yml").read_text()

    def test_no_write_permission_secret_or_publish_operation(self):
        self.assertRegex(self.text, r"(?m)^permissions:\n  contents: read$")
        self.assertEqual(len(re.findall(r"(?m)^\s*permissions:", self.text)), 1)
        for forbidden in ("secrets.", "contents: write", "pull_request_target", "workflow_run:",
                          "persist-credentials: true", "git push", "--force", "secrets: inherit",
                          "rafcodephi-publish-dev-apt.yml", "actions/cache@"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, self.text)

    def test_all_external_actions_are_pinned(self):
        actions = re.findall(r"(?m)^\s*(?:- )?uses:\s*(\S+)", self.text)
        self.assertTrue(actions)
        for action in actions:
            self.assertRegex(action, r"^actions/(checkout|upload-artifact)@[0-9a-f]{40}$")
        self.assertEqual(self.text.count("persist-credentials: false"), 4)

    def test_candidate_depends_on_validated_intake(self):
        intake = self.text.split("\n  intake:\n", 1)[1].split("\n  candidate:\n", 1)[0]
        candidate = self.text.split("\n  candidate:\n", 1)[1]
        self.assertIn("if: github.event_name == 'repository_dispatch'", intake)
        self.assertIn("needs: contract-tests", intake)
        self.assertIn("needs: intake", candidate)
        self.assertIn("ref: ${{ needs.intake.outputs.commit_sha }}", candidate)
        self.assertNotIn("github.event.client_payload", self.text)
        self.assertNotIn("github.event.sender", self.text)

    def test_existing_publisher_has_no_external_intake_trigger(self):
        publisher = (ROOT / ".github/workflows/rafcodephi-publish-dev-apt.yml").read_text()
        for trigger in ("repository_dispatch:", "workflow_run:", "workflow_call:"):
            self.assertNotIn(trigger, publisher)
        self.assertIn("production_signing_root=TOKEN_VAZIO", publisher)
        self.assertIn("claim_allowed_release=false", publisher)


if __name__ == "__main__":
    unittest.main()
