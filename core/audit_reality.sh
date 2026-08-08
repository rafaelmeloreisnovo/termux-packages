#!/usr/bin/env bash
# REAL: Audit script that classifies each core/*.c module as
#   REAL, SIMULATED, STUB, or TOKEN_VAZIO
# based on heuristic signals in the source.
#
# Item #12 from consolidation list.
# Emits a report to stdout and a JSON to core/tests/fixtures/reality_audit.json

set -uo pipefail

CORE_DIR="${1:-core}"
OUT_JSON="${CORE_DIR}/tests/fixtures/reality_audit.json"

mkdir -p "$(dirname "$OUT_JSON")"

# Modules known to be REAL (measured this session)
declare -A KNOWN_REAL=(
  ["pkg_scanner.c"]=1
  ["pkg_parser.c"]=1
  ["pkg_dag.c"]=1
  ["pkg_real_cli.c"]=1
)

classify() {
  local file="$1"
  local base
  base="$(basename "$file")"

  if [[ -n "${KNOWN_REAL[$base]:-}" ]]; then
    echo "REAL"
    return
  fi

  # Count heuristic signals
  local sim_hits stub_hits mock_hits hard_hits rand_hits
  sim_hits=$(grep -icE 'simulate|Simulated|simulation' "$file" 2>/dev/null || true)
  stub_hits=$(grep -icE 'stub|TODO|FIXME|not implemented' "$file" 2>/dev/null || true)
  mock_hits=$(grep -icE 'mock|dummy|fake|placeholder' "$file" 2>/dev/null || true)
  hard_hits=$(grep -icE 'hardcoded|fixed value|magic number' "$file" 2>/dev/null || true)
  rand_hits=$(grep -cE '\brand\s*\(' "$file" 2>/dev/null || true)

  # Absence of real I/O = likely simulated
  local io_hits
  io_hits=$(grep -cE 'fopen|open\s*\(|stat\s*\(|read\s*\(|write\s*\(' "$file" 2>/dev/null || true)

  # Classification tree
  if (( stub_hits + mock_hits >= 3 )); then
    echo "STUB"
  elif (( sim_hits >= 2 )) || (( rand_hits >= 3 && io_hits == 0 )); then
    echo "SIMULATED"
  elif (( io_hits >= 2 && sim_hits == 0 && rand_hits == 0 )); then
    echo "REAL"
  elif (( sim_hits + rand_hits + mock_hits + stub_hits == 0 && io_hits >= 1 )); then
    echo "REAL"
  else
    echo "SIMULATED"
  fi
}

# Emit human report
printf "%-45s  %-12s  %s\n" "MODULE" "STATUS" "SIGNALS(sim/stub/mock/hard/rand/io)"
printf "%-45s  %-12s  %s\n" "$(printf '%.0s-' {1..45})" "----" "-----"

json_entries=()
counts_REAL=0
counts_SIMULATED=0
counts_STUB=0
counts_TOKEN_VAZIO=0

for f in "$CORE_DIR"/*.c; do
  [[ -f "$f" ]] || continue
  base="$(basename "$f")"
  status="$(classify "$f")"

  sim=$(grep -icE 'simulate|Simulated|simulation' "$f" 2>/dev/null || true)
  stub=$(grep -icE 'stub|TODO|FIXME' "$f" 2>/dev/null || true)
  mock=$(grep -icE 'mock|dummy|fake|placeholder' "$f" 2>/dev/null || true)
  hard=$(grep -icE 'hardcoded|fixed value' "$f" 2>/dev/null || true)
  rand=$(grep -cE '\brand\s*\(' "$f" 2>/dev/null || true)
  io=$(grep -cE 'fopen|open\s*\(|stat\s*\(|read\s*\(|write\s*\(' "$f" 2>/dev/null || true)

  printf "%-45s  %-12s  %d/%d/%d/%d/%d/%d\n" \
    "$base" "$status" "$sim" "$stub" "$mock" "$hard" "$rand" "$io"

  case "$status" in
    REAL)         counts_REAL=$((counts_REAL + 1)) ;;
    SIMULATED)    counts_SIMULATED=$((counts_SIMULATED + 1)) ;;
    STUB)         counts_STUB=$((counts_STUB + 1)) ;;
    TOKEN_VAZIO)  counts_TOKEN_VAZIO=$((counts_TOKEN_VAZIO + 1)) ;;
  esac

  json_entries+=("{\"file\":\"$base\",\"status\":\"$status\",\"sim\":$sim,\"stub\":$stub,\"mock\":$mock,\"hard\":$hard,\"rand\":$rand,\"io\":$io}")
done

total=$((counts_REAL + counts_SIMULATED + counts_STUB + counts_TOKEN_VAZIO))

echo ""
echo "=== Summary ==="
printf "REAL:         %3d / %d (%.1f%%)\n" "$counts_REAL" "$total" \
  "$(awk "BEGIN { printf \"%.1f\", ($counts_REAL/$total)*100 }")"
printf "SIMULATED:    %3d / %d (%.1f%%)\n" "$counts_SIMULATED" "$total" \
  "$(awk "BEGIN { printf \"%.1f\", ($counts_SIMULATED/$total)*100 }")"
printf "STUB:         %3d / %d (%.1f%%)\n" "$counts_STUB" "$total" \
  "$(awk "BEGIN { printf \"%.1f\", ($counts_STUB/$total)*100 }")"
printf "TOKEN_VAZIO:  %3d / %d (%.1f%%)\n" "$counts_TOKEN_VAZIO" "$total" \
  "$(awk "BEGIN { printf \"%.1f\", ($counts_TOKEN_VAZIO/$total)*100 }")"

# Write JSON
{
  echo "{"
  echo "  \"schema\": \"reality_audit_v1\","
  echo "  \"generated_by\": \"audit_reality.sh\","
  echo "  \"totals\": {"
  echo "    \"REAL\": $counts_REAL,"
  echo "    \"SIMULATED\": $counts_SIMULATED,"
  echo "    \"STUB\": $counts_STUB,"
  echo "    \"TOKEN_VAZIO\": $counts_TOKEN_VAZIO,"
  echo "    \"total\": $total"
  echo "  },"
  echo "  \"modules\": ["
  first=1
  for e in "${json_entries[@]}"; do
    if (( first == 1 )); then
      printf "    %s\n" "$e"
      first=0
    else
      printf "    ,%s\n" "$e"
    fi
  done
  echo "  ]"
  echo "}"
} > "$OUT_JSON"

echo ""
echo "JSON: $OUT_JSON"
