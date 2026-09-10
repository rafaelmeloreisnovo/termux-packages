#!/usr/bin/env bash
# Whole-fleet static audit for Termux package recipes.
# Intentionally continues after individual findings and groups them by cause code.
set -Eeuo pipefail

MODE=report
OUTPUT_DIR=
while (($#)); do
  case "$1" in
    --mode) MODE="${2:?missing mode}"; shift 2 ;;
    --output) OUTPUT_DIR="${2:?missing output dir}"; shift 2 ;;
    -h|--help)
      echo "usage: $0 [--mode report|gate] [--output DIR]"
      exit 0 ;;
    *) echo "ERROR: unknown argument: $1" >&2; exit 2 ;;
  esac
done
[[ "$MODE" == report || "$MODE" == gate ]] || { echo "ERROR: mode must be report|gate" >&2; exit 2; }

ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"
: "${OUTPUT_DIR:=$ROOT/artifacts/package-fleet-audit}"
mkdir -p "$OUTPUT_DIR"
PACKAGES="$OUTPUT_DIR/packages.tsv"
FINDINGS="$OUTPUT_DIR/findings.tsv"
SUMMARY="$OUTPUT_DIR/summary.txt"
printf 'channel\tpackage\trecipe\n' > "$PACKAGES"
printf 'severity\tcode\trecipe\tdetail\n' > "$FINDINGS"

errors=0 warnings=0 infos=0 recipes=0 channels=0
declare -A seen=()

finding() {
  local sev="$1" code="$2" file="$3" detail="$4"
  detail="${detail//$'\t'/ }"; detail="${detail//$'\r'/ }"; detail="${detail//$'\n'/ | }"
  printf '%s\t%s\t%s\t%s\n' "$sev" "$code" "$file" "$detail" >> "$FINDINGS"
  case "$sev" in
    ERROR) ((errors+=1)) ;;
    WARN) ((warnings+=1)) ;;
    INFO) ((infos+=1)) ;;
    *) exit 3 ;;
  esac
}

literal_field() {
  local file="$1" field="$2"
  grep -Eq "^[[:space:]]*${field}=" "$file" || \
    finding WARN "FIELD_NOT_LITERAL_${field}" "$file" "No literal ${field}= assignment; dynamic assignment remains possible."
}

for channel in packages root-packages x11-packages; do
  [[ -d "$channel" ]] || continue
  ((channels+=1))
  while IFS= read -r -d '' file; do
    ((recipes+=1))
    rel="${file#./}"
    pkg="$(basename "$(dirname "$file")")"
    printf '%s\t%s\t%s\n' "$channel" "$pkg" "$rel" >> "$PACKAGES"

    if [[ -n "${seen[$pkg]:-}" && "${seen[$pkg]}" != "$channel" ]]; then
      finding WARN DUPLICATE_PACKAGE_NAME "$rel" "Also present in ${seen[$pkg]}."
    else
      seen[$pkg]="$channel"
    fi

    syntax=""
    syntax="$(bash -n "$file" 2>&1)" || finding ERROR BASH_SYNTAX "$rel" "$syntax"

    conflict="$(grep -nE '^[[:space:]]*(<<<<<<< .+|=======[[:space:]]*|>>>>>>> .+)$' "$file" || true)"
    [[ -z "$conflict" ]] || finding ERROR MERGE_CONFLICT_MARKER "$rel" "$conflict"

    [[ ! -x "$file" ]] || finding ERROR EXECUTABLE_RECIPE "$rel" "build.sh has executable bit set."
    LC_ALL=C grep -q $'\r$' "$file" && finding ERROR CRLF "$rel" "CRLF line ending detected."

    if [[ ! -s "$file" ]]; then
      finding ERROR EMPTY_RECIPE "$rel" "build.sh is empty."
    elif [[ "$(tail -c1 "$file" | od -An -tx1 | tr -d '[:space:]')" != 0a ]]; then
      finding ERROR NO_FINAL_NEWLINE "$rel" "Recipe is not newline terminated."
    fi

    while IFS= read -r rev; do
      [[ -z "$rev" ]] && continue
      ((10#$rev >= 1 && 10#$rev <= 999999999)) || \
        finding ERROR REVISION_RANGE "$rel" "TERMUX_PKG_REVISION=$rev outside 1..999999999."
    done < <(sed -nE 's/^[[:space:]]*TERMUX_PKG_REVISION=([0-9]+)[[:space:]]*$/\1/p' "$file")

    while IFS= read -r ver; do
      [[ -z "$ver" ]] && continue
      if command -v dpkg >/dev/null 2>&1 && ! dpkg --validate-version "$ver" >/dev/null 2>&1; then
        finding ERROR VERSION_SYNTAX "$rel" "Invalid literal TERMUX_PKG_VERSION=$ver."
      fi
    done < <(sed -nE \
      -e "s/^[[:space:]]*TERMUX_PKG_VERSION='([^']+)'[[:space:]]*$/\\1/p" \
      -e 's/^[[:space:]]*TERMUX_PKG_VERSION="([^"$]+)"[[:space:]]*$/\1/p' \
      -e 's/^[[:space:]]*TERMUX_PKG_VERSION=([A-Za-z0-9.+:~_-]+)[[:space:]]*$/\1/p' "$file")

    while IFS= read -r sha; do
      [[ -z "$sha" ]] && continue
      [[ "$sha" == SKIP_CHECKSUM || "$sha" =~ ^[0-9a-f]{64}$ ]] || \
        finding ERROR SHA256_LITERAL "$rel" "TERMUX_PKG_SHA256 must be exactly 64 lowercase hex characters or SKIP_CHECKSUM."
    done < <(sed -nE \
      -e "s/^[[:space:]]*TERMUX_PKG_SHA256='([^']+)'[[:space:]]*$/\\1/p" \
      -e 's/^[[:space:]]*TERMUX_PKG_SHA256="([^"$]+)"[[:space:]]*$/\1/p' \
      -e 's/^[[:space:]]*TERMUX_PKG_SHA256=([A-Za-z0-9_]+)[[:space:]]*$/\1/p' "$file")

    grep -Eq '^[[:space:]]*TERMUX_PKG_SRCURL=.*example\.(com|org|net)' "$file" && \
      finding ERROR PLACEHOLDER_SOURCE_URL "$rel" "Source URL uses an RFC-reserved example domain."

    for field in TERMUX_PKG_HOMEPAGE TERMUX_PKG_DESCRIPTION TERMUX_PKG_LICENSE TERMUX_PKG_MAINTAINER TERMUX_PKG_VERSION; do
      literal_field "$rel" "$field"
    done

    while IFS= read -r desc; do
      ((${#desc} <= 100)) || finding WARN DESCRIPTION_POLICY_DRIFT "$rel" \
        "Description has ${#desc} characters; legacy linter reports >100 but does not fail the recipe."
    done < <(sed -nE \
      -e "s/^[[:space:]]*TERMUX_PKG_DESCRIPTION='([^']*)'[[:space:]]*$/\\1/p" \
      -e 's/^[[:space:]]*TERMUX_PKG_DESCRIPTION="([^"$]*)"[[:space:]]*$/\1/p' "$file")
  done < <(find "$channel" -mindepth 2 -maxdepth 2 -type f -name build.sh -print0 | sort -z)
done

((recipes > 0)) || finding ERROR NO_RECIPES TOKEN_VAZIO "No package recipes discovered."

LINTER=scripts/lint-packages.sh
if [[ -f "$LINTER" ]]; then
  grep -A3 -F 'check_version() {' "$LINTER" | grep -qE '^[[:space:]]*return[[:space:]]*$' && \
    finding WARN TOOLING_VERSION_GATE_DISABLED "$LINTER" "check_version() returns before validation."
  grep -qF 'origin/master..' "$LINTER" && \
    finding WARN TOOLING_BASE_BRANCH_HARDCODED "$LINTER" "Legacy linter assumes origin/master while this fork uses main."
  grep -qF 'TERMUX_PKG_REVISION > 1 || TERMUX_PKG_REVISION < 999999999' "$LINTER" && \
    finding WARN TOOLING_REVISION_RANGE_LOGIC "$LINTER" "Revision range uses OR; fleet audit applies a strict range."
  grep -qF '[[ ! "$sha256" =~ [0-9a-f]{64} ]]' "$LINTER" && \
    finding WARN TOOLING_SHA256_UNANCHORED "$LINTER" "SHA-256 regex is unanchored; fleet audit anchors it."
  grep -qF 'before the first error was detected' "$LINTER" && \
    finding WARN TOOLING_STOP_AFTER_FIRST_ERROR "$LINTER" "Legacy traversal stops after first failing recipe; fleet audit does not."
else
  finding ERROR MISSING_LEGACY_LINTER "$LINTER" "Expected linter missing."
fi

{ head -n1 "$PACKAGES"; tail -n+2 "$PACKAGES" | LC_ALL=C sort; } > "$PACKAGES.tmp" && mv "$PACKAGES.tmp" "$PACKAGES"
{ head -n1 "$FINDINGS"; tail -n+2 "$FINDINGS" | LC_ALL=C sort -t $'\t' -k1,1 -k2,2 -k3,3; } > "$FINDINGS.tmp" && mv "$FINDINGS.tmp" "$FINDINGS"

p_hash="$(sha256sum "$PACKAGES" | awk '{print $1}')"
f_hash="$(sha256sum "$FINDINGS" | awk '{print $1}')"
head_sha="$(git rev-parse HEAD)"
{
  echo 'RAFCODEPHI PACKAGE FLEET AUDIT'
  echo "head=$head_sha"
  echo "mode=$MODE"
  echo "channels=$channels"
  echo "recipes=$recipes"
  echo "errors=$errors"
  echo "warnings=$warnings"
  echo "infos=$infos"
  echo "packages_sha256=$p_hash"
  echo "findings_sha256=$f_hash"
  echo "claim_state=$([[ $errors -eq 0 ]] && echo EVIDENCED_STATIC || echo REFUTED_STATIC)"
  echo 'claim_scope=STATIC_RECIPE_FLEET_ONLY'
  echo 'runtime_device=TOKEN_VAZIO'
  echo 'full_build_matrix=TOKEN_VAZIO'
  echo
  echo 'findings_by_code:'
  awk -F '\t' 'NR>1 {n[$2]++} END {for (k in n) printf "%s\t%d\n", k, n[k]}' "$FINDINGS" | LC_ALL=C sort
} > "$SUMMARY"
cat "$SUMMARY"

[[ "$MODE" != gate || $errors -eq 0 ]] || exit 1
