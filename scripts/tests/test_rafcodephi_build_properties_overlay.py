#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "scripts" / "apply-rafcodephi-build-properties.py"
spec = importlib.util.spec_from_file_location("raf_overlay", MODULE_PATH)
assert spec is not None and spec.loader is not None
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

BASE = '''\
TERMUX_APP__PACKAGE_NAME="com.termux"
TERMUX_APP_PACKAGE="$TERMUX_APP__PACKAGE_NAME"
TERMUX_REPO_APP__PACKAGE_NAME="com.termux"
'''


def expect_fail(text: str, token: str) -> None:
    try:
        mod.transform(text)
    except RuntimeError as exc:
        assert token in str(exc), (token, str(exc))
        return
    raise AssertionError(f"expected RuntimeError containing {token!r}")


def test_exact_single_replacement_and_repo_preservation() -> None:
    transformed, result = mod.transform(BASE)
    assert result["mode"] == "APPLIED"
    assert transformed.count(mod.TARGET_ASSIGNMENT) == 1
    assert transformed.count(mod.CURRENT_ASSIGNMENT) == 0
    assert transformed.count(mod.REPO_ASSIGNMENT) == 1
    # Only the app identity is eligible to change; the repo identity must stay upstream.
    before_lines = BASE.splitlines()
    after_lines = transformed.splitlines()
    changed = [(a, b) for a, b in zip(before_lines, after_lines) if a != b]
    assert changed == [(mod.CURRENT_ASSIGNMENT, mod.TARGET_ASSIGNMENT)]


def test_idempotent_after_exact_application() -> None:
    once, _ = mod.transform(BASE)
    twice, result = mod.transform(once)
    assert twice == once
    assert result["mode"] == "ALREADY_APPLIED"


def test_missing_upstream_assignment_fails_closed() -> None:
    expect_fail(BASE.replace(mod.CURRENT_ASSIGNMENT, ""), "APP_PACKAGE_ASSIGNMENT_NOT_UNIQUE")


def test_duplicate_upstream_assignment_fails_closed() -> None:
    expect_fail(BASE + mod.CURRENT_ASSIGNMENT + "\n", "APP_PACKAGE_ASSIGNMENT_NOT_UNIQUE")


def test_repo_identity_missing_fails_closed() -> None:
    expect_fail(BASE.replace(mod.REPO_ASSIGNMENT, ""), "REPO_IDENTITY_CONTRACT_INVALID")


def test_repo_identity_changed_fails_closed() -> None:
    expect_fail(
        BASE.replace(mod.REPO_ASSIGNMENT, 'TERMUX_REPO_APP__PACKAGE_NAME="com.termux.rafacodephi"'),
        "REPO_IDENTITY_CONTRACT_INVALID",
    )


def test_mixed_upstream_and_target_assignments_fails_closed() -> None:
    expect_fail(BASE + mod.TARGET_ASSIGNMENT + "\n", "TARGET_ASSIGNMENT_ALREADY_PRESENT")
