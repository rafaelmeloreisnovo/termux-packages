#!/usr/bin/env bash
# RAFCODEPHI package-fleet audit: scan the whole recipe fleet, classify failures by cause,
# and emit machine-readable evidence without pretending that a partial package build is fleet PASS.

set -Eeuo pipefail

MODE="report"
OUTPUT_DIR=""

usage() {
  cat <<'USAGE'
Usage: scripts/rafcodephi-package-fleet-audit.sh [--mode report|gate] [--output DIR]

Scans every build.sh under packages/, root-packages/, and x11-packages/.

Modes:
  report  Emit findings and always exit 0 after a successful scan.
  gate    Exit non-zero when ERROR findings exist.

Outputs:
  packages.tsv   every discovered recipe
  findings.tsv   normalized findings grouped by stable code
  summary.txt    counts, hashes, and claim state
USAGE
}

while (($#)); do
  case "$1" in
    --mode)
      [[ $# -ge 2 ]] || { echo "ERROR: --mode requires a value" >&2; exit 2; }
      MODE="$2"; shift 2 ;;
    --output)
      [[ $# -ge 2 ]] || { echo "ERROR: --output requires a value" >&2; exit 2; }
      OUTPUT_DIR="$2"; shift 2 ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "ERROR: unknown argument '$1'" >&2
      usage >&2
      exit 2 ;;
  esac
done

case "$MODE" in
  report|gate) ;;
  *) echo "ERROR: invalid mode '$MODE' (expected report|gate)" >&2; exit 2 ;;
esac

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
[[ -n "$ROOT" && -d "$ROOT/.git" ]] || {
  echo "ERROR: run this inside a git checkout" >&2
  exit 2
}
cd "$ROOT"

: "${OUTPUT_DIR:="$ROOT/artifacts/package-fleet-audit"}"
mkdir -p "$OUTPUT_DIR"
PACKAGES_TSV="$OUTPUT_DIR/packages.tsv"
FINDINGS_TSV="$OUTPUT_DIR/findings.tsv"
SUMMARY_TXT="$OUTPUT_DIR/summary.txt"

printf 'channel\tpackage\trecipe\n' > "$PACKAGES_TSV"
printf 'severity\tcode\trecipe\tdetail\n' > "$FINDINGS_TSV"

errors=0
warnings=0
infos=0
recipes=0
channels=0

declare -A seen_package_name=()

record_finding() {
  local severity="$1" code="$2" recipe="$3" detail="$4"
  detail="${detail//$'\t'/ }"
  detail="${detail//$'\r'/ }"
  detail="${detail//$'\n'/ | }"
  printf '%s\t%s\t%s\t%s\n' "$severity" "$code" "$recipe" "$detail" >> "$FINDINGS_TSV"
  case "$severity" in
    ERROR) ((errors+=1)) ;;
    WARN)  ((warnings+=1)) ;;
    INFO)  ((infos+=1)) ;;
    *) echo "INTERNAL ERROR: unknown severity '$severity'" >&2; exit 3 ;;
  esac
}

check_required_literal_field() {
  local recipe="$1" field="$2"
  if ! grep -Eq "^[[:space:]]*${field}=" "$recipe"; then
    record_finding WARN "FIELD_NOT_LITERAL_${field}" "$recipe" \
      "No literal ${field}= assignment found; dynamic assignment may be valid and needs semantic lint."
  fi
}

for channel in packages root-packages x11-packages; do
  [[ -d "$channel" ]] || continue
  ((channels+=1))

  while IFS= read -r -d '' recipe; do
    ((recipes+=1))
    rel="${recipe#./}"
    package="$(basename "$(dirname "$recipe")")"
    printf '%s\t%s\t%s\n' "$channel" "$package" "$rel" >> "$PACKAGES_TSV"

    if [[ -n "${seen_package_name[$package]:-}" && "${seen_package_name[$package]}" != "$channel" ]]; then
      record_finding WARN DUPLICATE_PACKAGE_NAME "$rel" \
        "Package name also exists in channel ${seen_package_name[$package]}; verify repository/channel ownership."
    else
      seen_package_name[$package]="$channel"
    fi

    syntax_error=""
    if ! syntax_error="$(bash -n "$recipe" 2>&1)"; then
      record_finding ERROR BASH_SYNTAX "$rel" "$syntax_error"
    fi

    if [[ -x "$recipe" ]]; then
      record_finding ERROR EXECUTABLE_RECIPE "$rel" "build.sh has executable bit set; package recipes are data sourced by build tooling."
    fi

    if LC_ALL=C grep -q $'\r$' "$recipe"; then
      record_finding ERROR CRLF "$rel" "CRLF line endings detected."
    fi

    if [[ -s "$recipe" ]]; then
      last_hex="$(tail -c 1 "$recipe" | od -An -tx1 | tr -d '[:space:]')"
      if [[ "$last_hex" != "0a" ]]; then
        record_finding ERROR NO_FINAL_NEWLINE "$rel" "Recipe is not newline-terminated."
      fi
    else
      record_finding ERROR EMPTY_RECIPE "$rel" "build.sh is empty."
    fi

    while IFS= read -r revision; do
      [[ -n "$revision" ]] || continue
      if ((10#$revision < 1 || 10#$revision > 999999999)); then
        record_finding ERROR REVISION_RANGE "$rel" \
          "TERMUX_PKG_REVISION=$revision is outside 1..999999999."
      fi
    done < <(sed -nE 's/^[[:space:]]*TERMUX_PKG_REVISION=([0-9]+)[[:space:]]*$/\1/p' "$recipe")

    while IFS= read -r version; do
      [[ -n "$version" ]] || continue
      if command -v dpkg >/dev/null 2>&1 && ! dpkg --validate-version "$version" >/dev/null 2>&1; then
        record_finding ERROR VERSION_SYNTAX "$rel" "Invalid literal TERMUX_PKG_VERSION=$version."
      fi
    done < <(sed -nE \
      -e "s/^[[:space:]]*TERMUX_PKG_VERSION='([^']+)'[[:space:]]*$/\\1/p" \
      -e 's/^[[:space:]]*TERMUX_PKG_VERSION="([^"$]+)"[[:space:]]*$/\1/p' \
      -e 's/^[[:space:]]*TERMUX_PKG_VERSION=([A-Za-z0-9.+:~_-]+)[[:space:]]*$/\1/p' \
      "$recipe")

    while IFS= read -r sha; do
      [[ -n "$sha" ]] || continue
      if [[ "$sha" != "SKIP_CHECKSUM" && ! "$sha" =~ ^[0-9a-f]{64}$ ]]; then
        record_finding ERROR SHA256_LITERAL "$rel" \
          "Literal TERMUX_PKG_SHA256 must be exactly 64 lowercase hex characters or SKIP_CHECKSUM."
      fi
    done < <(sed -nE \
      -e "s/^[[:space:]]*TERMUX_PKG_SHA256='([^']+)'[[:space:]]*$/\\1/p" \
      -e 's/^[[:space:]]*TERMUX_PKG_SHA256="([^"$]+)"[[:space:]]*$/\1/p' \
      -e 's/^[[:space:]]*TERMUX_PKG_SHA256=([A-Za-z0-9_]+)[[:space:]]*$/\1/p' \
      "$recipe")

    check_required_literal_field "$recipe" TERMUX_PKG_HOMEPAGE
    check_required_literal_field "$recipe" TERMUX_PKG_DESCRIPTION
    check_required_literal_field "$recipe" TERMUX_PKG_LICENSE
    check_required_literal_field "$recipe" TERMUX_PKG_MAINTAINER
    check_required_literal_field "$recipe" TERMUX_PKG_VERSION

    while IFS= read -r description; do
      if ((${#description} > 100)); then
        record_finding ERROR DESCRIPTION_TOO_LONG "$rel" \
          "Literal TERMUX_PKG_DESCRIPTION has ${#description} characters; repository limit is 100."
      fi
    done < <(sed -nE \
      -e "s/^[[:space:]]*TERMUX_PKG_DESCRIPTION='([^']*)'[[:space:]]*$/\\1/p" \
      -e 's/^[[:space:]]*TERMUX_PKG_DESCRIPTION="([^"$]*)"[[:space:]]*$/\1/p' \
      "$recipe")
  done < <(find "$channel" -mindepth 2 -maxdepth 2 -type f -name build.sh -print0 | sort -z)
done

if ((recipes == 0)); then
  record_finding ERROR NO_RECIPES TOKEN_VAZIO "No package recipes discovered in known channels."
fi

LINTER="scripts/lint-packages.sh"
if [[ -f "$LINTER" ]]; then
  if grep -A3 -F 'check_version() {' "$LINTER" | grep -qE '^[[:space:]]*return[[:space:]]*$'; then
    record_finding WARN TOOLING_VERSION_GATE_DISABLED "$LINTER" \
      "check_version() returns before validation; version-bump semantics are not enforced by the legacy linter."
  fi
  if grep -qF 'origin/master..' "$LINTER"; then
    record_finding WARN TOOLING_BASE_BRANCH_HARDCODED "$LINTER" \
      "Legacy linter assumes origin/master while this fork uses main as its canonical branch."
  fi
  if grep -qF 'TERMUX_PKG_REVISION > 1 || TERMUX_PKG_REVISION < 999999999' "$LINTER"; then
    record_finding WARN TOOLING_REVISION_RANGE_LOGIC "$LINTER" \
      "Legacy revision range condition uses OR and therefore accepts nearly every integer. Fleet audit compensates with a strict literal range check."
  fi
  if grep -qF '[[ ! "$sha256" =~ [0-9a-f]{64} ]]' "$LINTER"; then
    record_finding WARN TOOLING_SHA256_UNANCHORED "$LINTER" \
      "Legacy SHA-256 regex is not anchored. Fleet audit compensates with ^[0-9a-f]{64}$."
  fi
else
  record_finding ERROR MISSING_LEGACY_LINTER "$LINTER" "Expected repository linter is missing."
fi

{
  head -n1 "$PACKAGES_TSV"
  tail -n +2 "$PACKAGES_TSV" | LC_ALL=C sort -t $'\t' -k1,1 -k2,2 -k3,3
} > "$PACKAGES_TSV.tmp"
mv "$PACKAGES_TSV.tmp" "$PACKAGES_TSV"
{
  head -n1 "$FINDINGS_TSV"
  tail -n +2 "$FINDINGS_TSV" | LC_ALL=C sort -t $'\t' -k1,1 -k2,2 -k3,3
} > "$FINDINGS_TSV.tmp"
mv "$FINDINGS_TSV.tmp" "$FINDINGS_TSV"

packages_sha256="$(sha256sum "$PACKAGES_TSV" | awk '{print $1}')"
findings_sha256="$(sha256sum "$FINDINGS_TSV" | awk '{print $1}')"
head_sha="$(git rev-parse HEAD)"

{
  echo "RAFCODEPHI PACKAGE FLEET AUDIT"
  echo "head=$head_sha"
  echo "mode=$MODE"
  echo "channels=$channels"
  echo "recipes=$recipes"
  echo "errors=$errors"
  echo "warnings=$warnings"
  echo "infos=$infos"
  echo "packages_sha256=$packages_sha256"
  echo "findings_sha256=$findings_sha256"
  echo "claim_state=$([[ $errors -eq 0 ]] && echo EVIDENCED_STATIC || echo REFUTED_STATIC)"
  echo "claim_scope=STATIC_RECIPE_FLEET_ONLY"
  echo "runtime_device=TOKEN_VAZIO"
  echo "full_build_matrix=TOKEN_VAZIO"
  echo
  echo "findings_by_code:"
  awk -F '\t' 'NR > 1 {count[$2]++} END {for (code in count) printf "%s\t%d\n", code, count[code]}' "$FINDINGS_TSV" | LC_ALL=C sort
} > "$SUMMARY_TXT"

cat "$SUMMARY_TXT"

if [[ "$MODE" == "gate" && $errors -gt 0 ]]; then
  exit 1
fi
exit 0
