#!/bin/bash
set -e

PASS=0
FAIL=0
ROOT="$(cd "$(dirname "$0")" && pwd)"

check() {
    local label="$1"
    local output="$2"
    local pattern="$3"
    if echo "$output" | grep -qE "$pattern"; then
        echo "  PASS  $label"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  $label (expected pattern: $pattern)"
        echo "        got: $(echo "$output" | grep -E 'Fault|Coverage|coverage' | tail -3)"
        FAIL=$((FAIL + 1))
    fi
}

cd "$ROOT"

# ── Build main simulator ──────────────────────────────────────────────────────
echo "[build] main simulator"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1 && make -j4 > /dev/null 2>&1
cd "$ROOT"
SIM="$ROOT/build/simulator"

# ── c17: full ATPG should hit 100% ────────────────────────────────────────────
echo "[test] c17 ATPG (DALG v0)"
OUT=$(printf "READ ckts/c17.ckt\nTPG -rtpg v0 -atpg DALG DF-nl JF-v0 -out /dev/null\nQUIT\n" | $SIM 2>&1)
check "c17 100% coverage" "$OUT" "Fault Coverage: 1 "

# ── c17: PODEM standalone ─────────────────────────────────────────────────────
echo "[test] c17 PODEM standalone"
OUT=$(printf "READ ckts/c17.ckt\nLEV /dev/null\nPODEM 22 0 /dev/null\nQUIT\n" | $SIM 2>&1)
check "c17 PODEM runs" "$OUT" "OK|pattern|PODEM|==>|Test"

# ── c432: v0 ATPG should exceed 95% ──────────────────────────────────────────
echo "[test] c432 ATPG (DALG v0)"
OUT=$(printf "READ ckts/c432.ckt\nTPG -rtpg v0 -atpg DALG DF-nl JF-v0 -out /dev/null\nQUIT\n" | $SIM 2>&1)
check "c432 >= 95% coverage" "$OUT" "Fault Coverage: 0\.9[5-9]"

# ── BIST ──────────────────────────────────────────────────────────────────────
echo "[build] bist"
mkdir -p "$ROOT/bist/build" && cd "$ROOT/bist/build"
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1 && make -j4 > /dev/null 2>&1
cd "$ROOT"
BIST="$ROOT/bist/build/bist-simulator"

echo "[test] BIST c17"
OUT=$($BIST "$ROOT/ckts/c17.ckt" -n 1024 2>&1)
check "BIST c17 100% coverage" "$OUT" "100\.00%"

echo "[test] BIST c432"
OUT=$($BIST "$ROOT/ckts/c432.ckt" -n 2000 2>&1)
check "BIST c432 >= 95%" "$OUT" "9[5-9]\.[0-9][0-9]%"

echo "[test] BIST c880"
OUT=$($BIST "$ROOT/ckts/c880.ckt" -n 2000 2>&1)
check "BIST c880 >= 95%" "$OUT" "9[5-9]\.[0-9][0-9]%"

# ── Scan chain ────────────────────────────────────────────────────────────────
echo "[build] scan"
mkdir -p "$ROOT/scan/build" && cd "$ROOT/scan/build"
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1 && make -j4 > /dev/null 2>&1
cd "$ROOT"
SCAN="$ROOT/scan/build/scan-simulator"

echo "[test] scan s27"
OUT=$($SCAN "$ROOT/scan/benchmarks/s27.bench" -n 1000 2>&1)
check "scan s27 100% coverage" "$OUT" "100\.00%"

echo "[test] scan s_simple"
OUT=$($SCAN "$ROOT/scan/benchmarks/s_simple.bench" -n 1000 2>&1)
check "scan s_simple 100% coverage" "$OUT" "100\.00%"

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo "Results: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ] && exit 0 || exit 1
