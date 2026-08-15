#!/usr/bin/env python3
"""Fail closed when a GitHub workflow stops being a single valid YAML document."""

from __future__ import annotations

from pathlib import Path
import re
import subprocess
import sys

import yaml


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"
GITHUB_EXPRESSION = re.compile(r"\$\{\{.*?\}\}", re.DOTALL)


def fail(message: str) -> None:
    raise SystemExit(f"RAFCODEPHI_WORKFLOW_SYNTAX=BLOCKED {message}")


def bash_syntax_check(path: Path, job_name: str, step_index: int, step: dict, defaults: dict) -> None:
    script = step.get("run")
    if not isinstance(script, str):
        return

    job_defaults = defaults.get("run", {}) if isinstance(defaults, dict) else {}
    shell = step.get("shell", job_defaults.get("shell", "bash"))
    if not str(shell).startswith(("bash", "sh")):
        return

    normalized = GITHUB_EXPRESSION.sub("CI_EXPRESSION", script)
    result = subprocess.run(
        ["bash", "-n"],
        input=normalized,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode:
        name = step.get("name", "unnamed")
        fail(f"bash_parse path={path.relative_to(ROOT)} job={job_name} step={step_index} name={name}: {result.stderr.strip()}")


def validate_workflow(path: Path) -> int:
    try:
        documents = list(yaml.safe_load_all(path.read_text(encoding="utf-8")))
    except yaml.YAMLError as error:
        fail(f"yaml_parse path={path.relative_to(ROOT)}: {error}")

    if len(documents) != 1 or not isinstance(documents[0], dict):
        fail(f"single_document_required path={path.relative_to(ROOT)} documents={len(documents)}")

    workflow = documents[0]
    jobs = workflow.get("jobs")
    if not isinstance(jobs, dict) or not jobs:
        fail(f"jobs_mapping_required path={path.relative_to(ROOT)}")

    defaults = workflow.get("defaults", {})
    checked_steps = 0
    for job_name, job in jobs.items():
        if not isinstance(job, dict):
            fail(f"job_mapping_required path={path.relative_to(ROOT)} job={job_name}")
        job_defaults = job.get("defaults", defaults)
        steps = job.get("steps", [])
        if not isinstance(steps, list):
            fail(f"steps_list_required path={path.relative_to(ROOT)} job={job_name}")
        for step_index, step in enumerate(steps):
            if not isinstance(step, dict):
                fail(f"step_mapping_required path={path.relative_to(ROOT)} job={job_name} step={step_index}")
            bash_syntax_check(path, str(job_name), step_index, step, job_defaults)
            if isinstance(step.get("run"), str):
                checked_steps += 1
    return checked_steps


def main() -> int:
    files = sorted([*WORKFLOWS.glob("*.yml"), *WORKFLOWS.glob("*.yaml")])
    if not files:
        fail("workflow_files_missing")
    steps = sum(validate_workflow(path) for path in files)
    print(f"RAFCODEPHI_WORKFLOW_SYNTAX=PASS workflows={len(files)} run_steps={steps}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
