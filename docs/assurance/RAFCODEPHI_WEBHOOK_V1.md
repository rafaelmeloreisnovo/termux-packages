# RAFCODEPHI webhook intake v1

Authority: `rafaelmeloreisnovo/termux-packages`, package/source factory.
Scope: external request to approved source build and evidence for manual review.
`SOURCE != WEBHOOK != EXECUTION != EVIDENCE != CLAIM`; `claim_allowed=false`.

## Route and trust boundaries

1. An authorized external producer signs an exact UTF-8 JSON envelope.
2. `scripts/rafcodephi_webhook_receiver.py` verifies HMAC, schema, signed time,
   signed delivery identity, allowlists and a persistent SQLite reservation.
3. Its private credential posts a fixed `package_candidate` repository dispatch.
4. `.github/workflows/rafcodephi-webhook-intake.yml` checks the authenticated
   GitHub sender's numeric ID against `RAFCODEPHI_WEBHOOK_SENDER_ID`, parses the
   event from `GITHUB_EVENT_PATH`, and checks ancestry in the trusted main tree.
5. A separate read-only job checks out the validated exact SHA. Control code lives
   in another checkout. It runs provenance gates, recipe syntax, compilation of
   the manifest core, five fail-closed manifest cases, and the custom-prefix build.
6. Actual `.deb` metadata, architectures and hashes must match. Missing output,
   failed stage, symlink, wrong package or changed digest leaves promotion blocked.
7. Successful build/test receipts require manual review. There is no automatic
   connection to the privileged APT publisher.

The CI credential is `contents: read`; checkout credentials are not persisted.
No PAT, relay HMAC secret or publication credential is passed to candidate code.
The dispatcher credential remains exclusively on the external relay host. GitHub's
repository-dispatch API requires **Contents: write**, even when the resulting
workflow is read-only. Use a dedicated GitHub App installation token restricted
to this repository, rotated externally in the token file. A token with broader
repository access is outside this contract.

## Exact external envelope

```json
{
  "schema": "rafcodephi.webhook-event/v1",
  "event_type": "package_candidate",
  "delivery_id": "726f4469-7396-4e9d-b5be-8d132d5a60c6",
  "issued_at": 1800000000,
  "source_repo": "rafaelmeloreisnovo/termux-packages",
  "commit_sha": "10218c93dc72e5171da908ecc385155d2c8724c6",
  "package": "rafcodephi-federation-metadata",
  "architecture": "arm"
}
```

The timestamp and UUID above are examples; create a fresh UUIDv4 and current
integer Unix timestamp for a new authorized request. The SHA must be an exact
40-character lowercase commit already reachable from `main`. An unmerged PR
head is rejected even if its object exists in the repository.

`POST /webhook`, `Content-Type: application/json`, explicit `Content-Length`:

| Header | Value |
| --- | --- |
| `X-Hub-Signature-256` | `sha256=` plus lowercase HMAC-SHA256 of the exact body bytes |
| `X-GitHub-Delivery` | Same UUID as the signed `delivery_id` |
| `X-GitHub-Event` | Same signed `event_type`: `package_candidate` |

Timestamp and UUID are **inside the signed body**. Headers alone do not supply
freshness or replay protection. This is a custom producer envelope using the
GitHub HMAC convention; it is not a native GitHub `push` webhook payload. A native
webhook producer needs an explicit adapter that validates its own delivery and
creates this envelope. Do not substitute commit dates for delivery freshness.

Unknown/missing keys, duplicate JSON keys, nonfinite values, booleans as timestamps,
unapproved sources/packages/architectures and shell-like inputs are rejected.
The policy starts with federation metadata and bootstrap profile recipes, for
`arm` and `aarch64`. Expand only by reviewed policy changes. `package` names the
**producing recipe**; output-only subpackage names are not accepted implicitly.

Limits: 16 KiB body, maximum age 300 seconds, future clock skew 30 seconds.
CI permits up to 24 hours of queue delay after relay acceptance and revalidates
the original time/relay relation. It authenticates the GitHub sender; it cannot
independently recheck the relay's HMAC without receiving its secret.

## Durable replay and failure behavior

SQLite reserves the UUID and body SHA-256 in an atomic transaction **before**
network dispatch. The ledger uses full synchronous writes and preserves delivery
identifiers without TTL deletion. Transitions append `RESERVED`, then
`DISPATCH_ACCEPTED` or `DISPATCH_UNCERTAIN`. No raw body or secret enters the ledger.

| Observation | Result |
| --- | --- |
| Valid input, durable reservation, GitHub HTTP 204 | HTTP 202; dispatch accepted only |
| Invalid HMAC, schema, time, headers or framing | HTTP 400; no dispatch |
| Duplicate delivery or body | HTTP 409; no second dispatch |
| Database unavailable/full/locked beyond timeout | HTTP 503; fail closed |
| Timeout, redirect, HTTP error or non-204 response | HTTP 502; reservation remains consumed |
| Process crash after reservation | Reservation remains consumed; inspect provider state |

There is no claim of exactly-once GitHub execution: a crash after reservation may
lose a request, and workflow reruns remain possible. Ambiguous dispatches require
checking Actions runs for that delivery ID before a human-authorized new request.
Never release a reservation automatically to retry an uncertain POST. Never
restore an old ledger snapshot and then accept traffic without reconciling newer
deliveries. Stop ingress before backup/restore; use SQLite's backup API.

Use one durable database for all workers on one host. Independent local databases
on multiple hosts do not provide shared replay exclusion. Multi-host replication
is not implemented. Database growth and retention require an operator-controlled
archival plan that preserves used delivery IDs.

## Activation and rollback

The source change alone does not create a public endpoint or configure GitHub
Settings. The following bindings are required:

1. Review and merge this PR. `repository_dispatch` only runs a workflow present
   on the repository's default branch. `workflow_dispatch` here runs contract
   tests only; it does not accept arbitrary package inputs.
2. Supply a dedicated relay host with persistent disk and an HTTPS reverse proxy.
   Expose only `/webhook`; enforce 16 KiB request limits, rate limits, short header
   and body timeouts, TLS certificate verification, and no body/header logging.
   Preserve the exact signed body. The built-in server binds only to loopback and
   is not a standalone Internet-facing production server.
3. Provision two separate owner-readable files (mode `0600`): a random HMAC secret
   of at least 32 bytes and a dedicated repository-scoped GitHub dispatch token.
   Keep both outside this repository and all build workspaces. The sender receives
   only the HMAC secret. Token rotation must replace the token file atomically;
   the dispatcher rereads it on each request. Restart the receiver to rotate HMAC.
4. Set repository variable `RAFCODEPHI_WEBHOOK_SENDER_ID` to the observed numeric
   GitHub sender ID of that dedicated identity. Unset or mismatched identity blocks
   intake. A display name alone is not sufficient. The token holder can change
   repository contents; protect the credential accordingly.
5. Start the relay in the foreground from the reviewed checkout:

   ```sh
   python3 scripts/rafcodephi_webhook_receiver.py \
     --secret-file /secure/rafcodephi/webhook-secret \
     --token-file /secure/rafcodephi/github-dispatch-token \
     --ledger /var/lib/rafcodephi-webhook/deliveries.sqlite3
   ```

6. Send one freshly signed metadata-package request through HTTPS. Record HTTP
   status, ledger transition, Actions run ID, exact SHA, stage exits and artifact
   hashes. Redeliver identical bytes and verify HTTP 409 with no second dispatch.
7. Inspect the build receipt. `BUILD_TEST_PASS` enables **review**, not automatic
   publication, installation, Android execution or release/signing claims.

No host, URL, secret binding, settings mutation or live dispatch is implied by
local tests. Current external deployment and physical Android execution remain
`TOKEN_VAZIO` until their own receipts exist.

Rollback: stop ingress/relay; preserve the ledger; revoke/rotate the dedicated
dispatch token if needed; close the PR or revert its merge. The existing APT
publisher and package recipes are not changed by this increment. Its write token,
persisted credential and force-push behavior remain a separate hardening concern.
There is deliberately no `workflow_call`, `workflow_run` or external event wired
to the publisher.

## Evidence and reproducibility

```sh
python3 -m unittest discover -s scripts/tests -p 'test_rafcodephi_webhook*.py' -v
python3 scripts/validate_package_provenance_handoff.py
python3 scripts/validate_rafcodephi_federated_provenance.py
python3 scripts/tests/test_rafcodephi_publish_dev_apt_workflow_contract.py
make -C core termux-build-core
python3 core/tests/test_manifest_fail_closed.py
```

Runtime code uses Python 3.11+ standard library. Artifact tests also require
`git` and `dpkg-deb`; candidate build requires the existing Termux Docker builder.
The workflow uses fresh GitHub-hosted runners and no Actions build cache.
The existing builder image/toolchain resolution is inherited; this increment does
not claim hermetic or independently reproduced Android builds.

Receipts bind candidate repository/SHA, trusted control SHA, recipe SHA-256,
policy hash, relay body digest, each executed stage/exit/log hash, and actual
artifact package/architecture/size/SHA-256. A receipt hash proves byte identity,
not provenance of external source archives or correctness of a physical runtime.
`production_signing_root=TOKEN_VAZIO`, `publish_allowed=false`,
`claim_allowed=false` remain explicit even after a build passes.

Baseline independent gap: `tests/test_knowledge_work_seven_guards.py` fails on
`10218c93...` before execution with missing `re`; the validator also references
undefined `prov`/`ctx`. This increment preserves that failure and uses the
existing independently executable provenance and manifest gates.

Primary references:

- [GitHub: validate webhook deliveries](https://docs.github.com/en/webhooks/using-webhooks/validating-webhook-deliveries)
- [GitHub: webhook operational practices](https://docs.github.com/en/webhooks/using-webhooks/best-practices-for-using-webhooks)
- [GitHub: repository dispatch API permissions](https://docs.github.com/en/rest/repos/repos#create-a-repository-dispatch-event)
- [GitHub: repository dispatch default-branch semantics](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#repository_dispatch)
