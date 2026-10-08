# 555 bootstrap input custody · 2026-10-08

Scope: read-only observation of five attached files, not a signed GitHub Actions producer receipt. Owner: RAFAEL MELO REIS. Store only hashes/metadata, not private ZIP bytes.

| Fact | ARMv7 | AArch64 |
|---|---|---|
| ZIP | `rafcodephi-bootstrap-arm.zip` | `rafcodephi-bootstrap-aarch64.zip` |
| observed SHA256 | `73d706bbc5f21e77c1748bb38e7a8de30a0b4bb1f2685e80902417743a69a607` | `6c76707f17d46725e46590a21fb0cba1b42dd81566a0ead616aa7c74a2a1d185` |
| observed bytes | 22614856 | 25532754 |
| accompanying repair JSON SHA256 | `4fb7418862bb062f603d8e0dc91ac36c7c143c91de7892e99e7e3a9f7b657445` | `54448c5d1b2dadbb03caafecd1e0a2430bf065453a7be0da6efc44472835229b` |
| claimed bytes in attached manifest | 22614856 | 25531744 |
| APT guard | `APT::Update::Pre-Invoke` exits 100 | no active `Pre-Invoke` blocker |
| `termux.sources` | `Enabled: no`; invalid placeholder URL | `Enabled: yes`; GitHub raw `rafcodephi-apt-preview/repo` |

Both ZIPs are openable and list `dash←./bin/sh` plus `termux-api-broadcast←./libexec/termux-api` in `SYMLINKS.txt`. No unsafe parent/absolute ZIP path was identified during listing. Opening the ZIP does not prove Android runtime.

## Gaps and falsifiers
- Both observed ZIP SHA-256 values differ from corresponding repair JSON hashes; this is **unreconciled identity**, not evidence of a bad binary.
- AArch64 ZIP exceeds the attached manifest size by 1010 bytes; its enabled APT signed source contradicts the attached manifest's `BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED` characterization. The files cannot be promoted as one uniform historical set.
- The `rafcodephi-apt-preview` branch was not returned by a connected GitHub branch lookup; actual reachability, signatures and `apt update` remain unproven.
- Historical GitHub run `37696663078` failed at package custody. Metadata and file names do not establish identical ZIP bytes from that run.
- DEB content/provenance, 205-count per architecture, signing, published repository and on-device runtime are `TOKEN_VAZIO` for this specific attachment group.

## Route 00→10→20→30→90, NO_REBUILD
0. Read producer run `37696663078` with expected HEAD `77625363a031a081edd1db93d3f7a48e9bfff7f9`; permit observation of same-repo PR artifact only after exact SHA gate.
1. Read back exact source-run artifact; inventory both architectures independently, never execute uploaded binaries.
2. Compare source-run ZIP hashes, bytes, repair JSONs, package-custody maps, and manifesto; fail closed on any mismatch.
3. Only after exact correspondence, independently test repository URL/signature and physical device receipt as separate claims.
4. Record new receipt as supersession rather than rewriting old evidence.

State `OBSERVED_UNRECONCILED`, `claim_allowed=false`, `release_allowed=false`, `compiled_in_this_run=false`, `device_runtime_proof=TOKEN_VAZIO`.

R3=<F_ok:hashes+APT-state local,F_gap:producer-link+repository+device,F_next:exact-run readback>.
