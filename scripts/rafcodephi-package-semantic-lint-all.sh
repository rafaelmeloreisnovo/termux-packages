#!/usr/bin/env bash
# Run the repository's own lint_package() against every recipe without stopping at first failure.
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
LEGACY=scripts/lint-packages.sh
[[ -f "$LEGACY" ]] || { echo "ERROR: missing $LEGACY" >&2; exit 2; }
: "${OUTPUT_DIR:=$ROOT/artifacts/package-semantic-fleet-lint}"
mkdir -p "$OUTPUT_DIR/failures"
RESULTS="$OUTPUT_DIR/results.tsv"
SUMMARY="$OUTPUT_DIR/summary.txt"
printf 'status\tchannel\tpackage\trecipe\tlog_sha256\n' > "$RESULTS"

# Load only definitions/setup from the legacy linter. Do not execute its linter_main(),
# because that path deliberately breaks at the first failing package.
defs="$(mktemp)"
trap 'rm -f "$defs" "$OUTPUT_DIR/.current.log"' EXIT
awk '/^echo "\[INFO\]: Starting build script linter/{exit} {print}' "$LEGACY" > "$defs"
# shellcheck source=/dev/null
. "$defs"

checked=0 passed=0 failed=0
for channel in packages root-packages x11-packages; do
	[[ -d "$channel" ]] || continue
	while IFS= read -r -d '' recipe; do
		((checked+=1))
		rel="${recipe#./}"
		pkg="$(basename "$(dirname "$recipe")")"
		log="$OUTPUT_DIR/.current.log"

		if lint_package "$rel" >"$log" 2>&1; then
			((passed+=1))
			printf 'PASS\t%s\t%s\t%s\t-\n' "$channel" "$pkg" "$rel" >> "$RESULTS"
		else
			((failed+=1))
			failure_log="$OUTPUT_DIR/failures/${channel}__${pkg}.log"
			mv "$log" "$failure_log"
			log_hash="$(sha256sum "$failure_log" | awk '{print $1}')"
			printf 'FAIL\t%s\t%s\t%s\t%s\n' "$channel" "$pkg" "$rel" "$log_hash" >> "$RESULTS"
		fi
		done < <(find "$channel" -mindepth 2 -maxdepth 2 -type f -name build.sh -print0 | sort -z)
done

results_hash="$(sha256sum "$RESULTS" | awk '{print $1}')"
head_sha="$(git rev-parse HEAD)"
{
	echo 'RAFCODEPHI PACKAGE SEMANTIC FLEET LINT'
	echo "head=$head_sha"
	echo "mode=$MODE"
	echo "checked=$checked"
	echo "passed=$passed"
	echo "failed=$failed"
	echo "results_sha256=$results_hash"
	echo "legacy_linter_sha256=$(sha256sum "$LEGACY" | awk '{print $1}')"
	echo "claim_state=$([[ $failed -eq 0 ]] && echo EVIDENCED_LEGACY_LINT || echo REFUTED_LEGACY_LINT)"
	echo 'claim_scope=LEGACY_LINT_PACKAGE_FIELDS_AND_LAYOUT'
	echo 'source_fetch=TOKEN_VAZIO'
	echo 'full_build_matrix=TOKEN_VAZIO'
	echo 'runtime_device=TOKEN_VAZIO'
} > "$SUMMARY"
cat "$SUMMARY"

[[ "$MODE" != gate || $failed -eq 0 ]] || exit 1
