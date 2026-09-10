# Termux packages for the Google Play build of Termux

This repository contains the packages used in the [Termux build on Google Play](https://play.google.com/store/apps/details?id=com.termux&hl=en).

It's currently mostly interesting if you are a developer looking into the changes necessary to make Termux compatible with the Google Play requirements.

Otherwise, please work on [https://github.com/termux/termux-packages](https://github.com/termux/termux-packages) instead - this repository regularly merges in changes from there.

See https://github.com/termux-play-store for more information, status and updates regarding Termux on Google Play.

## RAFCODEPHI provenance and third-party attribution

This fork also carries a RAFCODEPHI package/source lane for
`com.termux.rafacodephi`. That lane does not change the authorship of Termux or
of any package built by this repository.

- Package recipes and patches follow the license of the actual package, as
  defined by [`LICENSE.md`](./LICENSE.md); the tree must not be flattened into a
  single RAFCODEPHI license.
- Human-readable fork and third-party credits are maintained in
  [`docs/assurance/THIRD_PARTY_AND_FORK_ATTRIBUTION.md`](./docs/assurance/THIRD_PARTY_AND_FORK_ATTRIBUTION.md).
- Machine-readable source pins, upstream projects, licenses and integration
  modes are maintained in
  [`docs/assurance/rafcodephi-federated-components.v1.json`](./docs/assurance/rafcodephi-federated-components.v1.json).
- `rafcodephi-*-profile` packages are local metapackages: they declare dependency
  graphs and contain no vendored Termux, BLAKE3, Vectras, QEMU, AndroidX or
  Termux:API source.
- Any future external source transplant must preserve required upstream notices
  and record exact origin repository, commit, path and file-specific license.
  QEMU is explicitly path-aware because its source tree contains multiple
  compatible licenses.

The structural gate is:

```sh
python3 scripts/validate_rafcodephi_federated_provenance.py
```

Build success remains distinct from install/runtime/device evidence.

## Quick guide to how to build a package
Most developers should use a prebuilt docker image to get a correctly configured and isolated build environment. Start with:

```sh
./scripts/run-docker.sh
```

Now build a package with:

```sh
./build-package.sh -i <package-name>
```

where `<package-name>` is a package name, corresponding to a directory `packages/<package-name/` (so `bash` and `vim` are examples of package names).

## Quick guide to how to develop and patch a package
There are mainly two parts to iterating on a package:

1. Edit the `packages/<package-name/build.sh` build script and run builds iteratively as above.
2. Update package patches and run builds iteratively as above.

Patches are applied from `packages/<package-name/*.patch` files.

Once there is an existing build, a built `.deb` file will be created in the `output/` directory, as in `output/bash_5.2.26-2_aarch64.deb`. Transfer that to your device and install with `dpkg -i output/bash_5.2.26-2_aarch64.deb`.

Feel free to reach out with an issue or [#termux-google-play on Matrix](https://matrix.to/#/#termux-google-play:matrix.org) to discuss or get help!
