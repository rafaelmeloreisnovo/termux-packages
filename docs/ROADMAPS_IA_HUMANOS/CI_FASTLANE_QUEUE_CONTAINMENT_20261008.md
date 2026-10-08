# CI fastlane and queue containment — 2026-10-08

Copyright (c) 2026 Rafael Melo Reis. Retain original project license and provenance.

Auto producer allowed 210-minute source-sensitive push; signed APT could run 240 minutes on PR/push. Change: short producer contract and signed APT contract still automatic, full ARM+ARM64 ZIPRAF producer/fan-in/handoff and signed APT now only workflow_dispatch; D3 real ARM/QEMU gate runs main and ready PR (draft skips with transit). Existing evidence hash, publication consent, custody and provider protection unaffected. Contract falsifiers updated to match manual-only heavy gate. Main protected=false observed; manual proof NOT_RUN until run. Rollback by reverting PR.

## Steps for a human or AI operator

1. Keep work in a Draft PR; inspect quick source-contract results without promoting them to binary PASS.
2. Mark Ready for review to trigger all retained full PR/ABI tests that apply.
3. Review source and rights; merge only with required checks and provider branch-protection readback.
4. Invoke expensive producer/beta/publish workflows deliberately after source is stable; capture exact source SHA, run ID and artifacts.
5. Never call source-only, QEMU-only or CI-only evidence physical Android validation. Record TOKEN_VAZIO, NOT_RUN, FAIL or PASS precisely.

R3 = <F_ok: source gate/refactor on PR, F_gap: CI executed exact head, provider P0, hardware receipts, F_next: CI quick -> ready full -> explicit delivery -> device readback>.
