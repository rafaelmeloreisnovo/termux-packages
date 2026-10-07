#!/usr/bin/env python3
"""Bounded failure fingerprint for expensive RAFCODEPHI builds.

Observation-only: never substitutes for a build gate or upgrades a claim to PASS.
The input is processed once with O(1) retained log records, even for large logs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

RULES = (
    ("LEGACY_PREFIX", re.compile(r"embeds forbidden legacy prefix")),
    ("RECEIVER_ROUTE", re.compile(r"client does not target the RAFCODEPHI API receiver|removed service stub route")),
    ("SYMLINK", re.compile(r"compatibility symlink is missing|missing real bootstrap archive target|symlink evidence repair.*(?:BLOCKED|FAIL)", re.I)),
    ("APT_REPOSITORY", re.compile(r"apt repository is not safely blocked|apt update fail-closed hook is missing")),
    ("PACKAGE_GATE", re.compile(r"REAL_BOOTSTRAP_SOURCEBUILD=BLOCKED|RAFCODEPHI_REAL_BOOTSTRAP_BUILD=BLOCKED")),
    ("PRODUCER_GATE", re.compile(r"RAFCODEPHI_PRODUCER=BLOCKED|RAFCODEPHI_BOOTSTRAP_DOCKER=BLOCKED")),
    ("COMPILER_ERROR", re.compile(r"(?:^|\\s)(?:fatal error:|error:|CMake Error at|FAILED: )")),
    ("ACTIONS_ERROR", re.compile(r"##\\[error\\]")),
)


def scan(path: Path) -> dict:
    counts = {key: 0 for key, _ in RULES}
    examples: dict[str, dict] = {}
    sha = hashlib.sha256()
    lines = 0
    with path.open("rb") as fh:
        for raw in fh:
            sha.update(raw)
            lines += 1
            text = raw.decode("utf-8", "replace").rstrip("\r\n")
            for key, pattern in RULES:
                if pattern.search(text):
                    counts[key] += 1
                    # Retain the first source-relevant observation, not 100k warnings.
                    examples.setdefault(key, {"line": lines, "text": text[:300]})
                    break
    primary = next((key for key, _ in RULES if counts[key]), "TOKEN_VAZIO")
    return {
        "schema": "rafcodephi.failure-triage/v1",
        "state": "OBSERVED_FAILURE_CANDIDATE" if primary != "TOKEN_VAZIO" else "TOKEN_VAZIO",
        "primary_candidate": primary,
        "counts": {k: v for k, v in counts.items() if v},
        "examples": examples,
        "log_lines": lines,
        "log_sha256": sha.hexdigest(),
        "claim_allowed": False,
        "full_build_pass": False,
        "physical_device": "TOKEN_VAZIO",
    }


def self_test() -> None:
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp) / "build.log"
        p.write_text(
            "warning: equality comparison with extraneous parentheses\n"
            "arm libexec/termux-api-broadcast embeds forbidden legacy prefix /data/data/com.termux/files/usr\n"
            "RAFCODEPHI_PRODUCER=BLOCKED exit_code=1\n", encoding="utf-8"
        )
        r = scan(p)
        assert r["primary_candidate"] == "LEGACY_PREFIX", r
        assert r["log_lines"] == 3, r
        assert r["counts"]["PRODUCER_GATE"] == 1, r
    print("RAFCODEPHI_FAILURE_TRIAGE_SELF_TEST=PASS")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--log", type=Path)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--markdown", type=Path)
    args = ap.parse_args()
    if args.self_test:
        self_test()
        return 0
    if args.log is None or args.out is None:
        ap.error("--log and --out required unless --self-test")
    if args.log.is_file():
        report = scan(args.log)
    else:
        report = {
            "schema": "rafcodephi.failure-triage/v1",
            "state": "TOKEN_VAZIO",
            "primary_candidate": "LOG_MISSING",
            "claim_allowed": False,
            "full_build_pass": False,
            "physical_device": "TOKEN_VAZIO",
        }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if args.markdown:
        lines = [
            "### RAFCODEPHI build failure triage (observation only)",
            f"- Candidate: `{report['primary_candidate']}`",
            f"- Log state: `{report['state']}`",
            "- This is **not** a successful build, device receipt, or release permission.",
        ]
        for key, example in report.get("examples", {}).items():
            lines.append(f"- {key} — log line {example['line']}: `{example['text'][:160].replace(chr(96), chr(39))}`")
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("RAFCODEPHI_FAILURE_TRIAGE=" + str(report["primary_candidate"]))
    return 0  # A failed build remains failed; triage is observational only.


if __name__ == "__main__":
    raise SystemExit(main())
