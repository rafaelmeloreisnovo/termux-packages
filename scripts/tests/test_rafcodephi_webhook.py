#!/usr/bin/env python3
"""Adversarial contract, durable replay, HTTP, ancestry and artifact tests."""
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
import hashlib
import hmac
import http.client
from http.server import HTTPServer
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock
import urllib.error
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import validate_rafcodephi_webhook_event as contract
import rafcodephi_webhook_receiver as receiver
import build_rafcodephi_webhook_candidate as builder

NOW = 1800000000
SECRET = b"test-only-secret-with-at-least-32-bytes"


def event(**changes):
    value = {"schema": "rafcodephi.webhook-event/v1", "event_type": "package_candidate",
             "delivery_id": "726f4469-7396-4e9d-b5be-8d132d5a60c6", "issued_at": NOW,
             "source_repo": "rafaelmeloreisnovo/termux-packages", "commit_sha": "a" * 40,
             "package": "rafcodephi-federation-metadata", "architecture": "arm"}
    value.update(changes)
    return value


def signature(raw):
    return "sha256=" + hmac.new(SECRET, raw, hashlib.sha256).hexdigest()


def wrapped(candidate, sender=123):
    return {"action": "package_candidate", "sender": {"id": sender},
            "repository": {"full_name": "rafaelmeloreisnovo/termux-packages", "default_branch": "main"},
            "client_payload": {"candidate": candidate, "relay": {
                "raw_body_sha256": hashlib.sha256(contract.canonical(candidate)).hexdigest(), "received_at": NOW}}}


class SchemaTests(unittest.TestCase):
    def setUp(self):
        self.policy = contract.load_policy()

    def test_valid_boundaries(self):
        for offset in (0, -300, 30):
            contract.validate_event(event(issued_at=NOW + offset), self.policy, NOW)

    def test_rejects_expired_future_noninteger_time(self):
        for value in (NOW - 301, NOW + 31, True, float(NOW), str(NOW), None):
            with self.subTest(value=value), self.assertRaises(contract.InvalidEvent):
                contract.validate_event(event(issued_at=value), self.policy, NOW)

    def test_rejects_commands_untrusted_routes_and_ref_injection(self):
        cases = [dict(command="touch /tmp/injected"), dict(package="../bash"), dict(package="bash;id"),
                 dict(package="$(id)"), dict(package="bash\nA=B"), dict(package="bash"),
                 dict(commit_sha="main"), dict(commit_sha="-" + "a" * 39), dict(commit_sha="A" * 40),
                 dict(source_repo="attacker/fork"), dict(source_repo="https://api.github.com"),
                 dict(event_type="publish"), dict(delivery_id="not-a-uuid"), dict(architecture="x86_64"),
                 dict(schema="v0"), dict(claim_allowed=True)]
        for change in cases:
            with self.subTest(change=change), self.assertRaises(contract.InvalidEvent):
                contract.validate_event(event(**change), self.policy, NOW)

    def test_missing_fields_and_wrong_top_level(self):
        for field in contract.EVENT_FIELDS:
            value = event()
            del value[field]
            with self.subTest(field=field), self.assertRaises(contract.InvalidEvent):
                contract.validate_event(value, self.policy, NOW)
        for value in (None, [], "value", 0, True):
            with self.subTest(value=value), self.assertRaises(contract.InvalidEvent):
                contract.validate_event(value, self.policy, NOW)

    def test_rejects_ambiguous_or_oversized_json(self):
        for raw in (b'{"a":1,"a":2}', b'{"nested":{"x":0,"x":1}}', b'{"n":NaN}',
                    b'{"n":Infinity}', b'\xff', b'{} trailing', b' ' * 16385, b'', b'[' * 2000):
            with self.subTest(raw=raw[:25]), self.assertRaises(contract.InvalidEvent):
                contract.strict_json(raw)

    def test_policy_fail_closed(self):
        for change in (dict(claim_allowed=True), dict(target_repository="attacker/fork"),
                       dict(max_age_seconds=3600), dict(max_body_bytes=True), dict(allowed_packages=[])):
            with tempfile.TemporaryDirectory() as td:
                path = Path(td) / "policy.json"
                path.write_bytes(contract.canonical({**self.policy, **change}))
                with self.subTest(change=change), self.assertRaises(contract.InvalidEvent):
                    contract.load_policy(path)


class RelayTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / "ledger.sqlite3"
        self.calls = []
        self.policy = contract.load_policy()
        self.ledger = receiver.ReplayLedger(self.path)
        self.relay = receiver.Relay(self.policy, SECRET, self.ledger, self.calls.append, lambda: NOW)
        self.value = event()
        self.raw = contract.canonical(self.value)

    def accept(self, raw=None, **headers):
        raw = self.raw if raw is None else raw
        return self.relay.accept(raw, headers.get("signature", signature(raw)),
                                 headers.get("delivery", self.value["delivery_id"]),
                                 headers.get("kind", "package_candidate"))

    def test_github_official_hmac_vector(self):
        receiver.verify_signature(b"Hello, World!", "sha256=757107ea0eb2509fc211221cce984b8a37570b6d7586c22c46f4379c8b043e17",
                                  b"It's a Secret to Everybody")

    def test_only_exact_signed_bytes_are_accepted(self):
        for value in (None, "", "sha1=" + "a" * 40, "sha256=" + "0" * 64,
                      "sha256=" + "A" * 64, signature(self.raw) + "\n"):
            with self.subTest(signature=value), self.assertRaises(contract.InvalidEvent):
                self.accept(signature=value)
        with self.assertRaises(contract.InvalidEvent):
            self.accept(raw=self.raw + b" ", signature=signature(self.raw))
        self.assertEqual(self.calls, [])

    def test_signed_payload_whitespace_is_preserved_for_hmac(self):
        raw = json.dumps(self.value, indent=2).encode()
        result = self.accept(raw=raw)
        self.assertEqual(result["raw_body_sha256"], hashlib.sha256(raw).hexdigest())
        self.assertEqual(self.calls[0]["client_payload"]["candidate"], self.value)

    def test_header_rebinding_does_not_bypass_signature(self):
        for headers in (dict(delivery=str(uuid.uuid4())), dict(kind="publish")):
            with self.subTest(headers=headers), self.assertRaises(contract.InvalidEvent):
                self.accept(**headers)
        self.assertEqual(self.calls, [])
        self.accept()  # Rejections did not consume a valid delivery.

    def test_timestamp_rewrite_requires_new_signature(self):
        changed = contract.canonical(event(issued_at=NOW + 1))
        with self.assertRaises(contract.InvalidEvent):
            self.accept(raw=changed, signature=signature(self.raw))
        self.assertEqual(self.calls, [])

    def test_authenticated_schema_failure_never_dispatches(self):
        for value in (event(issued_at=NOW - 301), event(command="id"), event(source_repo="evil/repo")):
            with self.subTest(value=value), self.assertRaises(contract.InvalidEvent):
                self.accept(raw=contract.canonical(value))
        self.assertEqual(self.calls, [])

    def test_replay_survives_restart(self):
        self.accept()
        self.relay.ledger = receiver.ReplayLedger(self.path)
        with self.assertRaises(receiver.DuplicateDelivery):
            self.accept()
        self.assertEqual(len(self.calls), 1)

    def test_concurrent_duplicate_has_one_dispatch(self):
        def attempt(_):
            try:
                self.accept()
                return "accepted"
            except receiver.DuplicateDelivery:
                return "duplicate"
        with ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(attempt, range(16)))
        self.assertEqual(results.count("accepted"), 1)
        self.assertEqual(results.count("duplicate"), 15)
        self.assertEqual(len(self.calls), 1)

    def test_ambiguous_dispatch_stays_consumed(self):
        self.relay.dispatch = mock.Mock(side_effect=TimeoutError("synthetic timeout"))
        with self.assertRaises(receiver.DispatchUncertain):
            self.accept()
        with self.assertRaises(receiver.DuplicateDelivery):
            self.accept()
        self.relay.dispatch.assert_called_once()
        with closing(sqlite3.connect(self.path)) as db:
            states = [r[0] for r in db.execute("SELECT state FROM transitions ORDER BY id")]
        self.assertEqual(states, ["RESERVED", "DISPATCH_UNCERTAIN"])

    def test_storage_failure_is_closed_before_dispatch(self):
        with mock.patch.object(self.ledger, "reserve", side_effect=sqlite3.OperationalError("disk full")):
            with self.assertRaises(sqlite3.OperationalError):
                self.accept()
        self.assertEqual(self.calls, [])

    def test_dispatch_accepted_is_not_build_evidence(self):
        result = self.accept()
        self.assertFalse(result["claim_allowed"])
        self.assertEqual(result["status"], "DISPATCH_ACCEPTED")
        self.assertEqual(set(self.calls[0]), {"event_type", "client_payload"})
        self.assertNotIn("command", json.dumps(self.calls))

    def test_http_acceptance_and_duplicate_over_real_loopback(self):
        server = HTTPServer(("127.0.0.1", 0), receiver.handler_for(self.relay))
        thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.01}, daemon=True)
        thread.start()
        try:
            headers = {"Content-Type": "application/json", "X-Hub-Signature-256": signature(self.raw),
                       "X-GitHub-Delivery": self.value["delivery_id"], "X-GitHub-Event": "package_candidate"}
            for expected in (202, 409):
                with closing(http.client.HTTPConnection(*server.server_address, timeout=2)) as connection:
                    connection.request("POST", "/webhook", self.raw, headers)
                    response = connection.getresponse()
                    self.assertEqual(response.status, expected)
                    self.assertFalse(json.loads(response.read())["claim_allowed"])
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)
        self.assertEqual(len(self.calls), 1)

    def test_http_rejects_ambiguous_headers_and_framing(self):
        cases = [("Content-Length", "1"), ("Transfer-Encoding", "chunked"), ("Content-Encoding", "gzip"),
                 ("Content-Type", "text/plain"), ("X-Hub-Signature-256", signature(self.raw))]
        server = HTTPServer(("127.0.0.1", 0), receiver.handler_for(self.relay))
        thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.01}, daemon=True)
        thread.start()
        try:
            for extra in cases:
                with self.subTest(extra=extra), closing(http.client.HTTPConnection(*server.server_address, timeout=2)) as connection:
                    connection.putrequest("POST", "/webhook")
                    for name, value in [("Content-Length", str(len(self.raw))), ("Content-Type", "application/json"),
                                        ("X-Hub-Signature-256", signature(self.raw)),
                                        ("X-GitHub-Delivery", self.value["delivery_id"]), ("X-GitHub-Event", "package_candidate"), extra]:
                        connection.putheader(name, value)
                    connection.endheaders(self.raw)
                    response = connection.getresponse()
                    self.assertEqual(response.status, 400)
                    response.read()
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)
        self.assertEqual(self.calls, [])


class DispatchClientTests(unittest.TestCase):
    def test_fixed_destination_rotatable_private_token_and_204(self):
        with tempfile.TemporaryDirectory() as td:
            token = Path(td) / "token"
            token.write_text("github_pat_" + "x" * 40)
            token.chmod(0o600)
            dispatcher = receiver.GitHubDispatcher("rafaelmeloreisnovo/termux-packages", token)
            response = mock.MagicMock()
            response.__enter__.return_value.status = 204
            dispatcher.opener = mock.Mock()
            dispatcher.opener.open.return_value = response
            dispatcher({"event_type": "package_candidate", "client_payload": {}})
            request = dispatcher.opener.open.call_args.args[0]
            self.assertEqual(request.full_url, "https://api.github.com/repos/rafaelmeloreisnovo/termux-packages/dispatches")
            self.assertEqual(request.get_method(), "POST")
            self.assertEqual(dispatcher.opener.open.call_args.kwargs["timeout"], 6)
            token.write_text("github_pat_" + "y" * 40)
            dispatcher({})
            self.assertTrue(dispatcher.opener.open.call_args.args[0].get_header("Authorization").endswith("y" * 40))
            response.__enter__.return_value.status = 200
            with self.assertRaises(receiver.DispatchUncertain):
                dispatcher({})

    def test_non_target_and_redirect_are_rejected(self):
        with self.assertRaises(contract.InvalidEvent):
            receiver.GitHubDispatcher("attacker/repo", Path("unused"))
        with self.assertRaises(receiver.DispatchUncertain):
            receiver.NoRedirect().redirect_request(None, None, 302, None, None, "https://attacker.invalid/")

    def test_secret_permissions_and_short_secret(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "secret"
            path.write_bytes(SECRET)
            path.chmod(0o644)
            with self.assertRaises(contract.InvalidEvent):
                receiver.read_private_file(path)
            path.chmod(0o600)
            self.assertEqual(receiver.read_private_file(path), SECRET)
            path.write_bytes(b"short")
            with self.assertRaises(contract.InvalidEvent):
                receiver.read_private_file(path)


class GitAndArtifactTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name) / "repo"
        self.root.mkdir()
        self.cmd("git", "init", "-b", "main")
        self.cmd("git", "config", "user.name", "Webhook Test")
        self.cmd("git", "config", "user.email", "test@example.invalid")
        recipe = self.root / "packages/rafcodephi-federation-metadata/build.sh"
        recipe.parent.mkdir(parents=True)
        recipe.write_text("TERMUX_PKG_VERSION=1.0\n")
        self.cmd("git", "add", ".")
        self.cmd("git", "commit", "-m", "fixture base")
        self.base = self.cmd("git", "rev-parse", "HEAD").strip()
        self.cmd("git", "checkout", "-b", "unreviewed")
        recipe.write_text("TERMUX_PKG_VERSION=999\n")
        self.cmd("git", "commit", "-am", "unreviewed fixture")
        self.untrusted = self.cmd("git", "rev-parse", "HEAD").strip()
        self.cmd("git", "checkout", "main")
        (self.root / "approved").write_text("trusted successor\n")
        self.cmd("git", "add", ".")
        self.cmd("git", "commit", "-m", "trusted fixture")
        self.head = self.cmd("git", "rev-parse", "HEAD").strip()
        self.policy = contract.load_policy()

    def cmd(self, *argv):
        return subprocess.check_output(argv, cwd=self.root, stderr=subprocess.DEVNULL, text=True)

    def validate(self, candidate=None, sender_id="123", wrapper=None):
        return contract.validate_dispatch(wrapper or wrapped(candidate or event(commit_sha=self.base)),
                                          self.policy, self.root, self.head, sender_id, NOW)

    def test_approved_ancestor_binds_actual_recipe(self):
        receipt = self.validate()
        self.assertEqual(receipt["recipe_sha256"], hashlib.sha256(b"TERMUX_PKG_VERSION=1.0\n").hexdigest())
        self.assertEqual(receipt["trusted_commit"], self.head)
        self.assertEqual(receipt["build_status"], "NOT_RUN")
        self.assertFalse(receipt["publish_allowed"])

    def test_relay_payload_is_consumable_by_intake(self):
        calls = []
        candidate = event(commit_sha=self.base)
        raw = contract.canonical(candidate)
        ledger = receiver.ReplayLedger(Path(self.tmp.name) / "handoff.sqlite3")
        relay = receiver.Relay(self.policy, SECRET, ledger, calls.append, lambda: NOW)
        relay.accept(raw, signature(raw), candidate["delivery_id"], candidate["event_type"])
        wrapper = wrapped(candidate)
        wrapper["client_payload"] = calls[0]["client_payload"]
        receipt = self.validate(wrapper=wrapper)
        self.assertEqual(receipt["candidate"], candidate)
        self.assertEqual(receipt["relay"]["raw_body_sha256"], hashlib.sha256(raw).hexdigest())

    def test_real_failing_provenance_stops_before_build_and_emits_blocked_receipt(self):
        current = int(time.time())
        candidate = event(commit_sha=self.head, issued_at=current)
        wrapper = wrapped(candidate)
        wrapper["client_payload"]["relay"]["received_at"] = current
        intake = contract.validate_dispatch(wrapper, self.policy, self.root, self.head, "123", current)
        reports = Path(self.tmp.name) / "build-reports"
        with mock.patch.object(builder, "ROOT", self.root):
            receipt = builder.build_candidate(intake, self.root, reports)
        self.assertEqual(receipt["status"], "BLOCKED")
        self.assertEqual(receipt["promotion"], "BLOCKED")
        self.assertFalse(receipt["publish_allowed"])
        self.assertEqual(len(receipt["stages"]), 1)
        self.assertEqual(receipt["stages"][0]["name"], "package-provenance")
        self.assertEqual(receipt["stages"][0]["status"], "FAIL")
        self.assertEqual(json.loads((reports / "receipt.json").read_text()), receipt)

    def test_cli_rejects_wrong_event_ref_and_produces_failure_receipt(self):
        path = Path(self.tmp.name) / "event.json"
        path.write_bytes(contract.canonical(wrapped(event(commit_sha=self.base))))
        for name, ref in (("workflow_dispatch", "refs/heads/main"), ("repository_dispatch", "refs/heads/other")):
            output = Path(self.tmp.name) / "cli-receipt.json"
            result = subprocess.run([sys.executable, str(contract.ROOT / "scripts/validate_rafcodephi_webhook_event.py"),
                                     "--github-event", str(path), "--receipt", str(output), "--root", str(self.root)],
                                    env={**os.environ, "GITHUB_EVENT_NAME": name, "GITHUB_REF": ref},
                                    capture_output=True, text=True, check=False)
            with self.subTest(name=name, ref=ref):
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(json.loads(output.read_text())["status"], "BLOCKED")

    def test_unreviewed_or_missing_commit_never_qualifies(self):
        for sha in (self.untrusted, "e" * 40):
            with self.subTest(sha=sha), self.assertRaises(contract.InvalidEvent):
                self.validate(event(commit_sha=sha))

    def test_sender_unconfigured_wrong_or_boolean_is_rejected(self):
        for sender_id in ("", "0", "999", "123\n", None):
            with self.subTest(sender_id=sender_id), self.assertRaises(contract.InvalidEvent):
                self.validate(sender_id=sender_id)
        with self.assertRaises(contract.InvalidEvent):
            self.validate(wrapper=wrapped(event(commit_sha=self.base), True))

    def test_repository_action_relay_and_queue_age_are_checked(self):
        for mutate in (lambda w: w.update(action="publish"),
                       lambda w: w["repository"].update(full_name="evil/fork"),
                       lambda w: w["repository"].update(default_branch="unreviewed"),
                       lambda w: w["client_payload"].update(command="id"),
                       lambda w: w["client_payload"]["relay"].update(raw_body_sha256="bad"),
                       lambda w: w["client_payload"]["relay"].update(received_at=NOW + 31)):
            value = wrapped(event(commit_sha=self.base))
            mutate(value)
            with self.subTest(value=value), self.assertRaises(contract.InvalidEvent):
                self.validate(wrapper=value)
        with self.assertRaises(contract.InvalidEvent):
            contract.validate_dispatch(wrapped(event(commit_sha=self.base)), self.policy, self.root,
                                       self.head, "123", NOW + 86401)

    def test_recipe_symlink_rejected_even_in_approved_tree(self):
        recipe = self.root / "packages/rafcodephi-federation-metadata/build.sh"
        recipe.unlink()
        recipe.symlink_to("../../approved")
        self.cmd("git", "add", ".")
        self.cmd("git", "commit", "-m", "symlink falsifier")
        self.head = self.cmd("git", "rev-parse", "HEAD").strip()
        with self.assertRaises(contract.InvalidEvent):
            self.validate(event(commit_sha=self.head))

    def create_deb(self, package="rafcodephi-federation-metadata", arch="all"):
        tree = Path(self.tmp.name) / "deb-tree"
        (tree / "DEBIAN").mkdir(parents=True, exist_ok=True)
        (tree / "DEBIAN/control").write_text(f"Package: {package}\nVersion: 1.0\nArchitecture: {arch}\nMaintainer: Fixture <test@example.invalid>\nDescription: synthetic artifact gate fixture\n")
        output = self.root / "output"
        output.mkdir(exist_ok=True)
        path = output / "fixture.deb"
        self.cmd("dpkg-deb", "--build", "--root-owner-group", str(tree), str(path))
        return path

    def test_real_deb_digest_and_metadata_gate(self):
        path = self.create_deb()
        artifacts = builder.artifacts_for(self.root, event())
        builder.verify_artifacts(self.root, event(), artifacts)
        self.assertEqual(artifacts[0]["architecture"], "all")
        self.assertEqual(artifacts[0]["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
        path.write_bytes(path.read_bytes() + b"tamper")
        with self.assertRaises(contract.InvalidEvent):
            builder.verify_artifacts(self.root, event(), artifacts)

    def test_missing_wrong_package_architecture_and_symlink_block_promotion(self):
        with self.assertRaises(contract.InvalidEvent):
            builder.artifacts_for(self.root, event())
        for package, arch in (("different-package", "all"), (event()["package"], "amd64")):
            self.create_deb(package, arch)
            with self.subTest(package=package, arch=arch), self.assertRaises(contract.InvalidEvent):
                builder.artifacts_for(self.root, event())
        path = self.create_deb()
        moved = Path(self.tmp.name) / "outside.deb"
        path.rename(moved)
        path.symlink_to(moved)
        with self.assertRaises(contract.InvalidEvent):
            builder.artifacts_for(self.root, event())

    def test_failed_stage_preserves_exit_and_log_hash(self):
        reports = Path(self.tmp.name) / "reports"
        reports.mkdir()
        stage = builder.run_stage("negative", [sys.executable, "-c", "print('falsifier'); raise SystemExit(17)"],
                                  self.root, reports, dict(os.environ), 5)
        self.assertEqual(stage["exit_code"], 17)
        self.assertEqual(stage["status"], "FAIL")
        self.assertEqual(stage["log_sha256"], builder.file_sha256(reports / "negative.log"))


if __name__ == "__main__":
    unittest.main()
