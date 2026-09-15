#!/usr/bin/env bash
#
# Walkthrough of the whole flow, in the order the pieces were built.
# Pass -y to run start to finish without stopping between sections.

set -euo pipefail
cd "$(dirname "$0")"

AUTO=0
[[ "${1:-}" == "-y" ]] && AUTO=1

BOLD=$'\033[1m'; DIM=$'\033[2m'; RESET=$'\033[0m'

step() {
  echo
  echo "${BOLD}=== $* ===${RESET}"
  echo
}

pause() {
  if [[ $AUTO -eq 0 ]]; then
    read -rp "${DIM}[enter to continue]${RESET} " _ || true
  fi
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

cov() {  # cov <netlist> <patterns> <seed>
  ./bist/build/bist-simulator "$1" -n "$2" -seed "$3" \
    | awk '/Fault Coverage/ {print $3}'
}

step "1. Build"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build -j"$(nproc)" > /dev/null
cmake -S bist -B bist/build -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build bist/build -j"$(nproc)" > /dev/null
cmake -S scan -B scan/build -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build scan/build -j"$(nproc)" > /dev/null
echo "simulator, bist-simulator, scan-simulator, unit_tests built"
pause

step "2. Unit tests"
./build/tests/unit_tests 2>/dev/null | tail -3
pause

step "3. ATPG from scratch: every fault in c17 gets a pattern"
printf 'READ ckts/c17.ckt\nTPG -rtpg v0 -atpg DALG DF-nl JF-v0 -out %s/c17.pat\nQUIT\n' "$WORK" \
  | ./build/simulator | grep -E 'Fault Coverage|Total patterns' | tail -2
echo
echo "${DIM}D-algorithm, one targeted run per fault. 100% on c17.${RESET}"
pause

step "4. The problem: random patterns stall"
echo "c880, 1000 pseudo-random patterns from a 32-bit LFSR:"
echo
for s in 2654435761 1013904226 3668339987; do
  printf '  seed %-12s coverage %s\n' "$s" "$(cov ckts/c880.ckt 1000 $s)"
done
echo
echo "${DIM}Short of 100%, and that gap is what test points are for.${RESET}"
pause

step "5. Insert test points"
printf 'READ ckts/c880.ckt\nTPI 32 %s/c880_tp.ckt %s/c880_tp.csv -mode mixed -metric cop -weight 3\nQUIT\n' "$WORK" "$WORK" \
  | ./build/simulator | grep -E 'Test points|Area|Critical|PPA source'
pause

step "6. Same circuit, same patterns, after insertion"
echo "                     before      after"
for s in 2654435761 1013904226 3668339987; do
  b=$(cov ckts/c880.ckt 1000 $s)
  a=$(cov "$WORK/c880_tp.ckt" 1000 $s)
  printf '  seed %-12s %-10s  %s\n' "$s" "$b" "$a"
done
echo
echo "${DIM}The transformed netlist is a normal .ckt -- every downstream tool reads it.${RESET}"
pause

step "7. What the metric choice is worth"
echo "Same budget, ranked by SCOAP cost instead of COP detection probability:"
echo
for metric in cop scoap; do
  printf 'READ ckts/c880.ckt\nTPI 32 %s/m_%s.ckt %s/m_%s.csv -mode mixed -metric %s -weight 3\nQUIT\n' \
    "$WORK" "$metric" "$WORK" "$metric" "$metric" | ./build/simulator > /dev/null
  printf '  %-6s %s\n' "$metric" "$(cov "$WORK/m_$metric.ckt" 1000 2654435761)"
done
echo
echo "${DIM}SCOAP prices deterministic ATPG effort. Random patterns care about"
echo "detection probability, which is a different question -- docs/metrics.md.${RESET}"
pause

step "8. Scan insertion, for the sequential half"
./scan/build/scan-simulator scan/benchmarks/s27.bench -n 1000 \
  | grep -E 'Flip-Flops|Pseudo|Fault Coverage'
echo
echo "${DIM}4 real inputs become 7 controllable ones, and the sequential"
echo "problem collapses to a combinational one.${RESET}"
pause

step "Done"
cat <<EOF
The full budget sweep and the plots behind the README tables:

  python3 python/sweep.py --circuits c432 c880 c1355 c1908
  python3 python/plot.py  --results results/results.csv

Cadence is not wired up here -- Genus and Innovus are licensed tools and this
machine has neither. The area and timing numbers above come from the analytical
cell model in src/ppa.cpp. Point TPI at a parsed synthesis report with -ppa to
swap them for real ones; nothing else changes.
EOF
