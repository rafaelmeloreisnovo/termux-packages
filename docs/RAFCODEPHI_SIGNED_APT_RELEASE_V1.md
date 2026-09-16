# RAFCODEPHI Signed APT Release V1

State: `IMPLEMENTED_UNTESTED_ACTIVATION_GATED`  
Repository: `rafaelmeloreisnovo/termux-packages`  
Publication is manual only. No automatic push publication is permitted.

## Activation bindings

Before a production publication run, bind exactly:

- `RAFCODEPHI_APT_SIGNING_KEY_B64`: base64-encoded private OpenPGP signing key;
- `RAFCODEPHI_APT_SIGNING_FINGERPRINT`: expected 40-hex uppercase fingerprint of that key;
- `RAFCODEPHI_APT_PUBLISH_TOKEN`: repository-scoped token permitted to update only the APT publication branch.

The workflow refuses production publication if any binding is absent or the imported key fingerprint differs.

## Build/test mode

Pull requests and manual runs with `publish=false` create an ephemeral CI signing key and must prove:

1. source-built ARM32 package closure;
2. signed `Release`, `InRelease`, and `Release.gpg`;
3. signature verification;
4. `Signed-By` APT client configuration;
5. isolated `apt-get update` and package download without `trusted=yes`;
6. SHA-256 of repository files and portable repository bundle.

The ephemeral key is evidence of signing mechanics only. It is not a production trust root.

## Production publication

Publication requires a manual `workflow_dispatch` with `publish=true`.

The same production public key is embedded in the generated RAFCODEPHI bootstrap and used through:

[
Signed-By: /data/data/com.termux.rafacodephi/files/usr/etc/apt/keyrings/rafcodephi-archive-key.gpg
]

The repository contains:

- `.deb` packages;
- `Packages`;
- `Packages.gz`;
- `Release`;
- `InRelease`;
- `Release.gpg`;
- `rafcodephi-archive-key.gpg`;
- `rafcodephi-archive-key.asc`;
- `RAFCODEPHI_REPOSITORY_MANIFEST.txt`;
- `SHA256SUMS`.

The publication branch is updated without force-push. Existing branch history is preserved.

## Website handoff

A website repository may later mirror the already-signed `repo/` directory byte-for-byte. The site is a transport surface, not a signing authority.

Required site receipt:

[
SHA256(site/repo/file_i)=SHA256(termux-packages publication/file_i)
]

for every published file. A different site build must not regenerate `Release`, `InRelease`, signatures, package indexes, or keys.

## Boundary

`SIGNED_REPOSITORY_READY != PUBLISHED`  
`PUBLISHED != PHYSICAL_ANDROID_RUNTIME`  
`claim_allowed=false` until the corresponding device/runtime receipts exist.
