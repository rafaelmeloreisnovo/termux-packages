# Unified RAFCODE source and provisional receipt (R3)

Authority: `rafaelmeloreisnovo/termux-packages` — APT and native package producer. This is an additive hosted CI lane. Existing specialized workflows, security and test requirements remain unchanged.

The exact immutable owned Action refs are `verify-source@47e0ad95703009bfaaa902f605fd3bf69aae4b48` (reviewed default branch) and `evidence-receipt@885b6f4c4356d5e1cea352653cbe69a4e141d198` (unmerged **DRAFT** Actions PR#19, independently exercised by Mapa pilot). Neither ref is a mutable tag. The new falsifier tests only local README SHA256 agreement between Python and hosted sha256sum, plus a one-byte corruption counterexample.

PROVISIONAL_HOST_TEST binds exact checkout SHA + GitHub run ID/attempt + README artifact SHA256 + falsifier script SHA256 and exit 0. This is **not** package/application/physical/scientific test evidence. Provider run conclusion is not attested while job executes; standalone reviewer must later check run ID, source SHA, artifact bytes/hash and negative test semantics, provider rules and local authorities. Do not promote a source digest to runtime PASS.

Reviewer checklist: expected source SHA matches HEAD; exact owned Action SHA/rights; old domain jobs remain; negative mutation rejected; run concludes SUCCESS; artifact bytes match; independent review and branch protection confirmed; rollback scoped. Credentials are stripped before executing the falsifier; no extra marketplace Actions used. Host Git/Bash/Python are build-test dependencies, not the freestanding Android core. User data and private repository bytes are not published. `claim_allowed=false`, `PROMOTION=HOLD`, no delete/merge/release/auto-dispatch. Local adaptation only, preserve upstream copyrights/licenses.
