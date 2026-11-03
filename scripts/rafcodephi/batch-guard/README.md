# Batch guard: deterministic failure propagation

Entry from repository root:

```sh
bash scripts/rafcodephi/batch-guard/check.sh
```

This is a bounded hosted CI adapter, not a freestanding core. It executes the
candidate workflow's actual Bash bodies against offline command doubles. It
does not download packages, install software, publish a repository or run a
device. Runtime tests use Python standard library only; no new pip dependency.

## Relations and contract

| Producer | Consumer | Boundary |
| --- | --- | --- |
| `rafcodephi-batch-pipeline.yml` APT materialization | portable evidence bundle | exact step outcome must be `success`; directories alone cannot authorize PASS |
| APT resolution subshell | JSON result and job exit | first failed command stops the subshell; logging failure also blocks |
| `scripts/tests/test_rafcodephi_batch_runtime.py` | this check.sh and source-contract CI | 18 offline cases, arm/aarch64 metadata fixtures; no physical ABI claim |
| Existing package recipes and publisher | future real APT validation | unchanged; upstream provenance and licenses retained |

Failure blocks advancement. There is no retry, alternate trust source, unsigned
fallback or silent failover. A failed later bundle attempt removes the preceding
tar and checksum; failure evidence remains available through existing uploads.
Each test child has a 10-second deadline; the existing source-contract job has a
10-minute deadline. These are nested time bounds, not independent hardware
watchdogs. Rollback of the proposal is closing the draft; after merge, revert
only its commit and rerun the same fixtures. The baseline defects would return.

## Dependency and low-level boundary

| Component | Owner / scope / retained reason | Replacement gate |
| --- | --- | --- |
| Bash | GNU; hosted workflow control, GPL-3.0-or-later; actual runner version must be recorded per run | preserve exit/pipeline semantics before any C/ASM adapter replacement |
| Python | Python Software Foundation; PSF license; host test runner, no application runtime dependency | offline byte/exit equivalent test adapter, not a new package interpreter |
| GitHub Actions runner and checkout | GitHub; existing pinned checkout SHA in YAML; CI execution and source retrieval | source/identity/log/artifact parity before replacement |
| APT/dpkg/GnuPG/tar | existing hosted package/signing tools; unchanged; individual versions/licenses from build environment remain TOKEN_VAZIO in this receipt | real signed repository, negative trust tests, exact artifact custody |

No Java, classes, heap allocator, ELF/DEX emitter or NUMA policy was added.
ELF belongs to the native artifact/ABI gate; DEX and ART to the Android consumer;
NUMA requires an observed machine topology and measured workload. Substituting
those mechanisms for shell error handling would add scope without closing this
defect. The separate `termux-packages-rafcode` C experiment is not wired here.
Authorial freestanding migration requires a defined input/output contract and
equivalent behavior; it must not relabel retained third-party code.

## Evidence and limits

Baseline: `5be0f29ae21e917e1eaeb653982056ab1fb5ecf8`.
Receipt: [2026-11-03 hotfix](../../../docs/receipts/RAFCODEPHI_BATCH_FAIL_CLOSED_20261103.md).
License: [LICENSE.md](LICENSE.md).
No performance/cost improvement is claimed. Real build, package install,
physical Android, recovery/failover and independent watchdogs remain NOT_RUN.
`claim_allowed=false` for release/runtime claims.
