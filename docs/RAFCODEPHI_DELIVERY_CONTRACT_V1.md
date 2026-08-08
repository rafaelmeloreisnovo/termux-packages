# RAFCODEPHI Delivery Contract V1

Status: `GOVERNED_DRAFT`

`claim_allowed=false`

This contract defines the evidence chain required to turn package source into a
usable RAFCODEPHI Termux runtime. No later gate may retroactively prove an earlier
one, and no source-level PASS may be promoted to artifact/device PASS.

## Invariant

```text
source recipe
  -> source contract
  -> governed install surface
  -> .deb artifact
  -> repository metadata
  -> bootstrap payload
  -> APK/prefix installation
  -> shell execution
  -> pkg/apt transaction
  -> physical ARM32/ARM64 receipt
```

Every arrow is a gate. Missing evidence is `TOKEN_VAZIO`/`NOT_MEASURED`, never
implicit success.

## Gate D0 — source identity and provenance

Required evidence:

- exact Git commit/tree;
- `build-package.sh` hash;
- package recipe hash;
- `TERMUX_APP_PACKAGE=com.termux.rafacodephi` contract;
- prefix `/data/data/com.termux.rafacodephi/files/usr` contract;
- architecture mapping `arm -> armeabi-v7a`, `aarch64 -> arm64-v8a`.

Current governed mechanisms:

- `scripts/emit_rafcodephi_bootstrap_source_manifest.py`;
- `scripts/rafcodephi_delivery_gate.py source`.

A D0 PASS proves source identity only.

## Gate D1 — essential package source contract

For Bash, minimum evidence is:

- non-placeholder version;
- HTTPS source URL;
- 64-hex SHA-256;
- `TERMUX_PKG_ESSENTIAL=true`;
- dependency on `termux-tools`.

For `termux-tools`, minimum evidence is:

- essential package flag;
- package/prefix variables exported into the build;
- package-id rewriting based on `TERMUX_APP_PACKAGE`.

Current state can be validated without building a package. This does not prove a
Debian artifact.

## Gate D2 — governed source install surface

Authoritative files:

- `core/product_surface.v1.json`;
- `scripts/install_core_governed.sh`.

The governed core surface is intentionally smaller than `make all` and does not
use the legacy broad `make install` route.

Allowed binaries:

```text
termux-build-core
manifest-dumper
```

Known test/fixture/prototype surfaces are excluded. The CI test installs into an
isolated `DESTDIR` and requires an exact file-name match.

A D2 PASS proves only a source-built installation tree on the CI host.

## Gate D3 — Debian artifact

Required command:

```bash
python3 scripts/rafcodephi_delivery_gate.py artifact \
  --repo . \
  --artifact output/<package>.deb \
  --expected-package <package-name> \
  --arch arm|aarch64
```

Required evidence:

- artifact exists;
- `dpkg-deb` parses it;
- Package field equals the expected package;
- Architecture matches the requested target;
- Version is non-empty;
- content listing is non-empty;
- SHA-256 and byte size are recorded.

Until a concrete `.deb` is supplied to this gate:

```text
D3 = NOT_MEASURED / TOKEN_VAZIO_ARTIFACT
```

Source readiness is not a substitute.

## Gate D4 — APT repository metadata

Required evidence for a real package repository includes, at minimum:

- exact set of `.deb` artifacts by hash;
- generated `Packages`/compressed index appropriate to the repository tooling;
- Release metadata appropriate to the repository policy;
- repository URL or local repository root;
- architecture/component mapping;
- signature/key policy when signing is claimed;
- independent parsing of generated metadata.

No claim is allowed merely because `.deb` files exist in a directory.

Current state:

```text
D4 = NOT_MEASURED
```

## Gate D5 — bootstrap payload

Required evidence:

- bootstrap input manifest linked to exact `.deb` hashes;
- expected package closure for the selected ABI;
- exact prefix/package id;
- deterministic archive inventory and hash;
- extraction safety checks;
- no silent file collision/truncation;
- receipt connecting source commit -> artifacts -> bootstrap hash.

A bootstrap that materializes files is not equivalent to an installed, usable
prefix.

Current source-profile tooling exists; runtime/bootstrap artifact evidence is a
separate gate.

## Gate D6 — installed prefix and shell execution

On the actual RAFCODEPHI application environment, record:

```text
package id
ABI
Android version
kernel/uname
prefix path
bootstrap hash
bash .deb hash
termux-tools .deb hash
command path
ELF interpreter / dynamic dependencies when applicable
exit code
stdout/stderr hash or captured receipt
```

Minimum positive execution:

```bash
/data/data/com.termux.rafacodephi/files/usr/bin/bash --version
/data/data/com.termux.rafacodephi/files/usr/bin/bash -lc 'printf "RAF_BASH_OK\n"'
```

A CI-host Bash execution is not a physical Android receipt.

Current state:

```text
D6 = NOT_MEASURED
```

## Gate D7 — `pkg` / `apt` transaction

Required evidence:

- configured repository sources inside the RAFCODEPHI prefix;
- successful metadata refresh;
- package resolution;
- download hash verification;
- `dpkg` unpack/configure completion;
- post-install executable path;
- second execution after installation;
- negative test for unavailable/corrupt package metadata.

Minimum transaction example, once a governed repository exists:

```text
apt update
apt install <governed-test-package>
<installed-command> --version
```

Network availability alone does not satisfy this gate.

Current state:

```text
D7 = NOT_MEASURED
```

## Gate D8 — independent ARM32 / ARM64 physical receipts

ARM32 and ARM64 are independent claims. A PASS on one does not promote the
other. Each receipt must bind:

- source commit;
- package artifacts;
- bootstrap artifact;
- APK/application identity;
- ABI/device identity;
- runtime commands and exit codes;
- timestamp;
- hashes.

The architecture nominal matrix and QEMU availability are not substitutes for
these receipts.

Current state:

```text
D8_ARM32 = NOT_MEASURED
D8_ARM64 = NOT_MEASURED
```

## Promotion rule

```text
SOURCE_READY
!= ARTIFACT_READY
!= REPOSITORY_READY
!= BOOTSTRAP_READY
!= PREFIX_READY
!= SHELL_READY
!= PKG_APT_READY
!= DEVICE_VERIFIED
```

For a claim such as "RAFCODEPHI has a usable Bash/pkg runtime on Android", the
minimum promotion requirement is a continuous evidence path through all gates
that the claim depends on.

## Current F_ok / F_gap / F_next

`F_ok`:

- real Termux package builder exists;
- Bash and termux-tools have real essential recipes;
- package/prefix source contract exists;
- governed minimal core install surface exists;
- source delivery gate is executable and fail-closed;
- artifact gate is executable and rejects missing artifacts.

`F_gap`:

- concrete ARM/AArch64 `.deb` artifact receipts;
- repository metadata receipt;
- bootstrap artifact receipt;
- installed-prefix receipt;
- Bash execution on the RAFCODEPHI Android environment;
- `pkg`/`apt` end-to-end transaction;
- independent ARM32 and ARM64 device receipts.

`F_next`:

Produce the first real essential `.deb`, run D3 on it, and only then construct
D4 from the validated artifact set.
