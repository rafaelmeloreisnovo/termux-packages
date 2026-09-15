#!/usr/bin/env python3
"""Strict shared webhook contract and trusted-checkout GitHub intake. Stdlib only."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_POLICY = ROOT / "configs/rafcodephi-webhook-events.v1.json"
SHA40 = re.compile(r"[0-9a-f]{40}")
SHA256 = re.compile(r"[0-9a-f]{64}")
PACKAGE = re.compile(r"[a-z0-9][a-z0-9+.-]{0,79}")
DELIVERY = re.compile(r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")
EVENT_FIELDS = {"schema", "event_type", "delivery_id", "issued_at", "source_repo", "commit_sha", "package", "architecture"}


class InvalidEvent(ValueError):
    """A bounded, credential-free diagnostic code."""


def require(condition, code):
    if not condition:
        raise InvalidEvent(code)


def exact_keys(value, expected, code):
    require(isinstance(value, dict) and set(value) == set(expected), code)


def matches(pattern, value):
    return isinstance(value, str) and pattern.fullmatch(value) is not None


def _unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate_json_key")
        result[key] = value
    return result


def strict_json(raw, limit=16384):
    require(isinstance(raw, bytes) and 0 < len(raw) <= limit, "body_size")
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=_unique,
                          parse_constant=lambda _: (_ for _ in ()).throw(InvalidEvent("nonfinite_json")))
    except (UnicodeError, json.JSONDecodeError, RecursionError, ValueError) as error:
        raise InvalidEvent("invalid_json") from error


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("ascii")


def load_policy(path=DEFAULT_POLICY):
    policy = strict_json(Path(path).read_bytes())
    exact_keys(policy, {"schema", "target_repository", "trusted_branch", "event_type", "allowed_packages",
                        "allowed_architectures", "max_body_bytes", "max_age_seconds", "future_skew_seconds",
                        "max_queue_age_seconds", "claim_allowed"}, "policy_fields")
    require(policy["schema"] == "rafcodephi.webhook-policy/v1", "policy_schema")
    require(policy["target_repository"] == "rafaelmeloreisnovo/termux-packages", "policy_repository")
    require(policy["trusted_branch"] == "main" and policy["event_type"] == "package_candidate", "policy_route")
    require(policy["claim_allowed"] is False, "policy_claim")
    for key, ceiling in (("max_body_bytes", 16384), ("max_age_seconds", 300),
                         ("future_skew_seconds", 30), ("max_queue_age_seconds", 86400)):
        require(type(policy[key]) is int and 0 < policy[key] <= ceiling, "policy_limit")
    packages = policy["allowed_packages"]
    require(isinstance(packages, list) and 0 < len(packages) <= 32, "policy_packages")
    require(all(matches(PACKAGE, p) for p in packages) and len(set(packages)) == len(packages), "policy_package")
    arches = policy["allowed_architectures"]
    require(isinstance(arches, list) and bool(arches) and all(a in ("arm", "aarch64") for a in arches), "policy_architecture")
    return policy


def validate_event(event, policy, now=None, max_age=None):
    exact_keys(event, EVENT_FIELDS, "event_fields")
    require(event["schema"] == "rafcodephi.webhook-event/v1", "event_schema")
    require(event["event_type"] == policy["event_type"], "event_type")
    require(event["source_repo"] == policy["target_repository"], "source_repository")
    require(matches(DELIVERY, event["delivery_id"]), "delivery_id")
    require(matches(SHA40, event["commit_sha"]), "commit_sha")
    require(matches(PACKAGE, event["package"]) and event["package"] in policy["allowed_packages"], "package_allowlist")
    require(isinstance(event["architecture"], str) and event["architecture"] in policy["allowed_architectures"], "architecture_allowlist")
    require(type(event["issued_at"]) is int, "issued_at_type")
    now = int(time.time()) if now is None else now
    age = policy["max_age_seconds"] if max_age is None else max_age
    require(-policy["future_skew_seconds"] <= now - event["issued_at"] <= age, "event_expired_or_future")
    return event


def git(root, *args):
    result = subprocess.run(["git", "-C", str(root), *args], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False, timeout=30)
    require(result.returncode == 0, "git_identity_or_ancestry")
    return result.stdout


def bind_candidate(event, root, trusted_sha):
    require(matches(SHA40, trusted_sha), "trusted_sha")
    require(git(root, "rev-parse", "HEAD").decode().strip() == trusted_sha, "control_checkout_mismatch")
    commit = event["commit_sha"]
    require(git(root, "cat-file", "-t", commit).strip() == b"commit", "candidate_not_commit")
    # No payload-controlled fetch URL, ref, shell, submodule, or code runs here.
    git(root, "merge-base", "--is-ancestor", commit, trusted_sha)
    recipe = f"packages/{event['package']}/build.sh"
    tree = git(root, "ls-tree", commit, "--", recipe).decode().strip().split()
    require(len(tree) == 4 and tree[0] in ("100644", "100755") and tree[1] == "blob"
            and tree[3] == recipe, "recipe_not_regular_blob")
    digest = hashlib.sha256(git(root, "show", f"{commit}:{recipe}")).hexdigest()
    return {"recipe_path": recipe, "recipe_sha256": digest, "trusted_commit": trusted_sha}


def validate_dispatch(wrapper, policy, root, trusted_sha, sender_id, now=None):
    require(isinstance(sender_id, str) and re.fullmatch(r"[1-9][0-9]{0,19}", sender_id), "sender_allowlist_unconfigured")
    require(isinstance(wrapper, dict), "github_event")
    sender = wrapper.get("sender", {})
    repository = wrapper.get("repository", {})
    require(isinstance(sender, dict) and type(sender.get("id")) is int and str(sender["id"]) == sender_id, "sender_not_allowed")
    require(isinstance(repository, dict) and repository.get("full_name") == policy["target_repository"]
            and repository.get("default_branch") == policy["trusted_branch"], "github_repository")
    require(wrapper.get("action") == policy["event_type"], "github_action")
    payload = wrapper.get("client_payload")
    exact_keys(payload, {"candidate", "relay"}, "dispatch_fields")
    event = validate_event(payload["candidate"], policy, now, policy["max_queue_age_seconds"])
    relay = payload["relay"]
    exact_keys(relay, {"raw_body_sha256", "received_at"}, "relay_fields")
    require(matches(SHA256, relay["raw_body_sha256"]) and type(relay["received_at"]) is int, "relay_metadata")
    current = int(time.time()) if now is None else now
    require(-policy["future_skew_seconds"] <= current - relay["received_at"] <= policy["max_queue_age_seconds"], "dispatch_expired")
    validate_event(event, policy, relay["received_at"])
    identity = bind_candidate(event, root, trusted_sha)
    return {"schema": "rafcodephi.webhook-intake-receipt/v1", "candidate": event, "relay": relay,
            **identity, "policy_sha256": hashlib.sha256(canonical(policy)).hexdigest(),
            "authentication": "GITHUB_SENDER_ALLOWLIST; HMAC_CHECKED_AT_RELAY",
            "status": "INTAKE_ACCEPTED", "build_status": "NOT_RUN", "device_runtime": "TOKEN_VAZIO",
            "publish_allowed": False, "claim_allowed": False}


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(canonical(value) + b"\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--github-event", type=Path, required=True)
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args()
    try:
        require(os.environ.get("GITHUB_EVENT_NAME") == "repository_dispatch", "github_event_name")
        policy = load_policy(args.policy)
        require(os.environ.get("GITHUB_REF") == "refs/heads/" + policy["trusted_branch"], "github_ref")
        wrapper = strict_json(args.github_event.read_bytes(), limit=131072)
        receipt = validate_dispatch(wrapper, policy, args.root, os.environ.get("GITHUB_SHA", ""),
                                    os.environ.get("RAFCODEPHI_WEBHOOK_SENDER_ID", ""))
        write_json(args.receipt, receipt)
        output = os.environ.get("GITHUB_OUTPUT")
        if output:
            # Every value has been validated against an anchored ASCII grammar/allowlist.
            with open(output, "a", encoding="utf-8") as stream:
                for key in ("commit_sha", "package", "architecture", "delivery_id"):
                    stream.write(f"{key}={receipt['candidate'][key]}\n")
                stream.write("receipt_json=" + canonical(receipt).decode("ascii") + "\n")
        print("WEBHOOK_INTAKE=PASS build=NOT_RUN publish_allowed=false claim_allowed=false")
        return 0
    except (InvalidEvent, OSError, subprocess.SubprocessError) as error:
        code = str(error) if isinstance(error, InvalidEvent) else "intake_io_failure"
        write_json(args.receipt, {"schema": "rafcodephi.webhook-intake-receipt/v1", "status": "BLOCKED",
                                  "reason": code, "build_status": "NOT_RUN", "publish_allowed": False,
                                  "claim_allowed": False})
        print(f"WEBHOOK_INTAKE=BLOCKED reason={code} claim_allowed=false")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
