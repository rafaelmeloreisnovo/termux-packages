#!/usr/bin/env bash
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"
mkdir -p build/reports

for cmd in dpkg-deb dpkg-scanpackages apt-ftparchive gpg apt-get apt-cache; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y dpkg-dev apt-utils gnupg
    break
  fi
done

tmp="$(mktemp -d "${RUNNER_TEMP:-/tmp}/rafcodephi-signed-apt-fixture.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
export GNUPGHOME="$tmp/gnupg"
mkdir -p "$GNUPGHOME"
chmod 700 "$GNUPGHOME"

gpg --batch --passphrase '' --quick-generate-key \
  "RAFCODEPHI Signed APT Fixture <fixture@rafcodephi.invalid>" ed25519 sign 1d >/dev/null 2>&1
fpr="$(gpg --batch --with-colons --list-secret-keys | awk -F: '$1=="fpr"{print $10; exit}')"
[[ "$fpr" =~ ^[0-9A-F]{40}$ ]]

pkgroot="$tmp/pkg"
mkdir -p "$pkgroot/DEBIAN" "$pkgroot/data/data/com.termux.rafacodephi/files/usr/share/rafcodephi"
cat > "$pkgroot/DEBIAN/control" <<'EOF'
Package: rafcodephi-custody-fixture
Version: 1.0.0
Architecture: arm
Maintainer: RAFCODEPHI CI
Description: Signed APT custody fixture
EOF
printf '%s\n' 'fixture=1' > "$pkgroot/data/data/com.termux.rafacodephi/files/usr/share/rafcodephi/fixture.txt"

repo="$tmp/repo"
mkdir -p "$repo"
dpkg-deb --build --root-owner-group "$pkgroot" "$repo/rafcodephi-custody-fixture_1.0.0_arm.deb" >/dev/null

(
  cd "$repo"
  LC_ALL=C dpkg-scanpackages . /dev/null > Packages
  gzip -9n -c Packages > Packages.gz
  LC_ALL=C apt-ftparchive release . > Release
  gpg --batch --export "$fpr" > rafcodephi-archive-key.gpg
  gpg --batch --yes --local-user "$fpr" --clearsign --output InRelease Release
  gpg --batch --yes --local-user "$fpr" --detach-sign --output Release.gpg Release
  gpg --batch --verify Release.gpg Release
  gpg --batch --verify InRelease
  sha256sum *.deb Packages Packages.gz Release InRelease Release.gpg rafcodephi-archive-key.gpg \
    | LC_ALL=C sort -k2 > SHA256SUMS
  sha256sum -c SHA256SUMS
)

aptroot="$tmp/apt"
mkdir -p "$aptroot/etc/apt/sourceparts" "$aptroot/lists/partial" "$aptroot/cache/archives/partial" "$aptroot/var/lib/dpkg"
: > "$aptroot/var/lib/dpkg/status"
printf 'deb [signed-by=%s/rafcodephi-archive-key.gpg] file:%s ./\n' "$repo" "$repo" > "$aptroot/etc/apt/sources.list"

opts=(
  -o "Dir::Etc::sourcelist=$aptroot/etc/apt/sources.list"
  -o "Dir::Etc::sourceparts=$aptroot/etc/apt/sourceparts"
  -o "Dir::State::status=$aptroot/var/lib/dpkg/status"
  -o "Dir::State::lists=$aptroot/lists"
  -o "Dir::Cache::archives=$aptroot/cache/archives"
  -o 'APT::Architecture=arm'
  -o 'APT::Architectures=arm'
  -o 'Acquire::Languages=none'
)

apt-get "${opts[@]}" update >/dev/null
apt-cache "${opts[@]}" show rafcodephi-custody-fixture | grep -Fx 'Package: rafcodephi-custody-fixture'
apt-get "${opts[@]}" -y --download-only --no-install-recommends install rafcodephi-custody-fixture \
  2>&1 | tee build/reports/rafcodephi-signed-apt-fixture-download.log
mapfile -t downloaded < <(find "$aptroot/cache/archives" -maxdepth 1 -type f -name '*.deb' -print | sort)
(( ${#downloaded[@]} >= 1 )) || { echo "BLOCKED: signed APT resolved package but no downloaded .deb was materialized" >&2; exit 1; }
dpkg-deb -f "${downloaded[0]}" Package | grep -Fx 'rafcodephi-custody-fixture'

sha256sum "$repo/rafcodephi-custody-fixture_1.0.0_arm.deb" "$repo/InRelease" "$repo/Release.gpg" \
  > build/reports/rafcodephi-signed-apt-fixture.sha256
cat > build/reports/rafcodephi-signed-apt-fixture.receipt <<EOF
schema=rafcodephi.signed-apt-fixture/v1
state=PASS
signing_mode=ephemeral-test
signing_fingerprint=$fpr
trust=SIGNED_BY_ARCHIVE_KEY
trusted_yes=false
claim_allowed_release=false
physical_android=TOKEN_VAZIO
EOF

echo "RAFCODEPHI_SIGNED_APT_FIXTURE=PASS fingerprint=$fpr"
