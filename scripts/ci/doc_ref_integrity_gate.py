#!/usr/bin/env python3
"""RAFAELIA documentation integrity V1: stdlib, read-only, no network.

Only Markdown inline file links are checked. Anchor presence, reference links,
HTML links and third-party destinations are NOT verified; PASS is scoped.
"""
from __future__ import annotations
import argparse
import json
import os
import posixpath
import re
import subprocess
from pathlib import Path
from urllib.parse import unquote, urlsplit

INLINE = re.compile(r"!?\[[^\]\n]*\]\(")
TITLE = re.compile(r"\s+['\"].*['\"]\s*$")
EXTERNAL = {"http", "https", "mailto", "tel", "data"}
SCHEMA = "rafaelia.docs.integrity.v1"
BT = chr(96)


def git(root: Path, *args: str) -> bytes:
    return subprocess.check_output(["git", *args], cwd=root, stderr=subprocess.DEVNULL)


def tracked_paths(root: Path) -> set[str]:
    return {os.fsdecode(item) for item in git(root, "ls-files", "-z").split(b"\0") if item}


def inline_targets(source: str):
    fence = None
    for line_no, line in enumerate(source.splitlines(), 1):
        stripped = line.lstrip()
        marker = stripped[:3]
        if marker in (BT * 3, "~~~"):
            if fence is None:
                fence = marker
            elif fence == marker:
                fence = None
            continue
        if fence:
            continue
        line = re.sub(BT + "[^" + BT + "]*" + BT, "", line)
        for match in INLINE.finditer(line):
            start = match.end()
            depth = 1
            position = start
            while position < len(line):
                char = line[position]
                if char == "\\":
                    position += 2
                    continue
                if char == "(":
                    depth += 1
                elif char == ")":
                    depth -= 1
                    if depth == 0:
                        yield line_no, line[start:position].strip()
                        break
                position += 1


def classify_target(root: Path, source: str, target: str, tracked: set[str]):
    if not target:
        return "IGNORED_EMPTY", ""
    if target.startswith("#"):
        return "SKIPPED_ANCHOR_UNVERIFIED", ""
    if target.startswith("<") and ">" in target:
        target = target[1:target.index(">")]
    else:
        target = TITLE.sub("", target).strip()
    parsed = urlsplit(target)
    if parsed.scheme:
        return ("SKIPPED_EXTERNAL_UNVERIFIED" if parsed.scheme.lower() in EXTERNAL
                else "UNSAFE_SCHEME"), target
    if target.startswith("//"):
        return "SKIPPED_EXTERNAL_UNVERIFIED", target
    if target.startswith("/") or target.startswith("\\"):
        return "ABSOLUTE_NOT_PORTABLE", target
    decoded = unquote(parsed.path).replace("\\ ", " ")
    if not decoded:
        return "SKIPPED_ANCHOR_UNVERIFIED", ""
    if "\0" in decoded or "\\" in decoded:
        return "INVALID_PATH", target
    path = posixpath.normpath(posixpath.join(posixpath.dirname(source), decoded))
    if path in ("..", ".") or path.startswith("../"):
        return "OUTSIDE_REPOSITORY", target
    root_resolved = root.resolve()
    candidate = (root / path).resolve(strict=False)
    if not candidate.is_relative_to(root_resolved):
        return "SYMLINK_ESCAPE", target
    if path in tracked or any(item.startswith(path.rstrip("/") + "/") for item in tracked):
        return "RESOLVED_TRACKED", path
    return "MISSING_TRACKED_TARGET", path


def root_inventory(paths: set[str]) -> dict:
    """Inventory tracked root directories, without declaring them abandoned."""
    root_files = sum(1 for p in paths if "/" not in p)
    dirs = sorted({p.split("/", 1)[0] for p in paths if "/" in p})
    index_names = ("README.md", "README.rst", "INDEX.md", "index.md", "FILES_MAP.md")
    missing = [d for d in dirs if not any(d + "/" + name in paths for name in index_names)]
    return {
        "tracked_files": len(paths), "root_files": root_files, "root_dirs": len(dirs),
        "missing_direct_index_count": len(missing),
        "missing_direct_index_sample": missing[:30],
        "precision": "DIRECT_INDEX_HEURISTIC_NOT_ORPHAN_PROOF"
    }


def audit(root: Path, documents: list[str], all_docs: bool = False) -> dict:
    root = root.resolve()
    paths = tracked_paths(root)
    if all_docs:
        documents = sorted(p for p in paths if p.lower().endswith(".md"))
    head = git(root, "rev-parse", "HEAD").decode("ascii").strip()
    findings = []
    total = checked = skipped = 0
    for doc in sorted(set(documents)):
        if doc not in paths or not doc.lower().endswith(".md"):
            findings.append({"file": doc, "line": 0, "status": "DOC_NOT_TRACKED_OR_NOT_MD"})
            continue
        file_path = root / doc
        if not file_path.resolve(strict=False).is_relative_to(root):
            findings.append({"file": doc, "line": 0, "status": "DOC_SYMLINK_ESCAPE"})
            continue
        try:
            content = file_path.read_text(encoding="utf-8")
        except (UnicodeError, OSError) as exc:
            findings.append({"file": doc, "line": 0, "status": "DOC_UNREADABLE",
                             "detail": type(exc).__name__})
            continue
        checked += 1
        for line_no, target in inline_targets(content):
            total += 1
            status, resolved = classify_target(root, doc, target, paths)
            if status.startswith("SKIPPED") or status == "IGNORED_EMPTY":
                skipped += 1
            elif status != "RESOLVED_TRACKED":
                findings.append({"file": doc, "line": line_no, "status": status, "target": resolved})
    return {
        "schema": SCHEMA, "source_head": head, "scope": "INLINE_MARKDOWN_FILE_LINKS_ONLY",
        "inventory": root_inventory(paths),
        "files_checked": checked, "links_seen": total, "unverified_or_skipped": skipped,
        "errors_total": len(findings), "findings": findings[:40],
        "findings_truncated": len(findings) > 40, "claim_allowed": False,
        "result": "PASS_SCOPED" if not findings else "FAIL_SCOPED"
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--file", action="append", dest="docs", default=[])
    parser.add_argument("--all", action="store_true", help="Scan all tracked Markdown")
    parser.add_argument("--audit-only", action="store_true", help="Report without blocking CI")
    args = parser.parse_args()
    if not args.docs and not args.all:
        parser.error("specify --file or --all")
    try:
        report = audit(args.root, args.docs, args.all)
    except (OSError, subprocess.CalledProcessError, UnicodeError) as exc:
        print(json.dumps({"schema": SCHEMA, "result": "FAIL_ENVIRONMENT",
                          "error": type(exc).__name__, "claim_allowed": False}))
        return 2
    print(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return 0 if args.audit_only or report["errors_total"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
