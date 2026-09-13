# Seven Guards Logic/Structure Exam — Termux Packages — 2026-09-13

state: SOURCE_HARDENING_CANDIDATE
claim_allowed: false

## Exam
V1 was challenged with malformed-but-complete inputs.

Observed bypass classes before hardening:
1. unknown contradiction state accepted;
2. unknown uncertainty state accepted;
3. unknown rollback state accepted when mutation=false;
4. arbitrary evidence type accepted;
5. malformed provenance object hash accepted;
6. branch-like GitHub ref accepted instead of exact commit;
7. arbitrary reconstruction pointer accepted.

## Development response
V1.1 adds closed vocabularies, exact GitHub commit validation, digest shape validation, canonical reconstruction pointer validation, and minimum coupling between reproduction PASS and reproduction-class evidence.

## Boundary
This is source/package assurance logic. It does not prove source fetch, package build, install, pkg runtime, signing or physical Android behavior.

## R3
F_ok: bypasses identified with falsifying inputs; hardening encoded in validator/tests.
F_gap: remote CI for candidate and producer->consumer/device execution.
F_next: run existing Package Provenance Handoff gate; preserve build/install/runtime separation.
