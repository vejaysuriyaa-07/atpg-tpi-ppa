# ATPG with PPA-Aware Test Point Insertion

This started as a from-scratch ATPG simulator — parse an ISCAS-85 netlist, run
fault simulation, implement DALG and PODEM properly instead of treating Tessent
and TetraMAX as black boxes. That part works and the numbers are below.

What it grew into is the part I actually find interesting. ATPG gets you to ~98%
coverage on most of these circuits, but BIST doesn't: random patterns stall out
because some lines are effectively impossible to hit by chance. The fix is test
point insertion — bolt on extra hardware to make those lines controllable or
observable. That hardware costs area and timing, so the question isn't "can I
raise coverage", it's **how much coverage per unit of silicon**, and where the
knee in that curve is.

So the inserter here picks test points, rewrites the netlist, and prices the
result against a cell model. Three of its design choices came out of
measurements that contradicted my first guess — those are written up in
[docs/metrics.md](docs/metrics.md), including the one where my first version
made coverage *worse*.

## Build and run the demo

```bash
./demo.sh          # builds everything, walks through the whole flow
./demo.sh -y       # same, no pauses
```

Needs cmake, a C++17 compiler, and Python 3 with pandas + matplotlib for the
plots. GoogleTest is fetched automatically if the system doesn't have it.

## What's here

| Piece | Where | What it does |
|---|---|---|
| ATPG core | `src/`, `includes/` | DALG, PODEM, SCOAP, fault sim, 5 TPG strategies |
| Test point insertion | `src/tpi.cpp` | selects points, rewrites the netlist, prices it |
| COP analysis | `src/cop.cpp` | signal/detection probabilities — drives the selection |
| PPA model | `src/ppa.cpp` | cell area and delay, swappable for synthesis data |
| BIST | `bist/` | LFSR/MISR self-test, used here as the measurement harness |
| Scan chain | `scan/` | full-scan insertion for sequential `.bench` circuits |
| Orchestration | `python/` | budget sweeps, ablations, plots |
| Tests | `tests/` | 22 GoogleTest cases |

## Test point insertion

```bash
TPI <budget> <out_netlist.ckt> <report.csv>
    [-mode cp|op|mixed]      # control points, observe points, or both
    [-metric cop|scoap]      # how candidates get ranked
    [-weight <n>]            # control point fires 1 time in 2^n
    [-bias <f>]              # tilt mixed mode toward observe points
    [-ppa <model_file>]      # use real synthesis numbers instead of the model
```

It writes a normal `.ckt`, so everything downstream — fault sim, ATPG, the BIST
harness — reads the transformed circuit with no special handling. That was the
main reason to do the transform as a netlist rewrite rather than an in-memory
patch: `Node` owns its fan-in and fan-out as raw arrays sized at parse time, and
growing the circuit in place means rebuilding all of it anyway.

An observe point taps a line out to a new output. A control point splices an
AND (force 0) or OR (force 1) between the line and its fanout, with a gated test
pin on the side input.

## How it picks points

Rank every internal line by the odds a random pattern detects a fault on it —
P(line sits at the needed value) × P(the resulting flip reaches an output) —
and spend the budget on the worst ones.

The first version ranked by SCOAP instead, since the simulator already computes
it. It barely helped. SCOAP measures how much work deterministic ATPG has to do
to reach a line, which is a different question from how likely a random vector
is to get there on its own. Switching to COP probabilities is most of why this
works; `-metric scoap` keeps the old behaviour so the two can be compared.

## Results

### ATPG core

Pure deterministic ATPG, one targeted DALG run per fault, no random fill
(`TPG -rtpg v0 -atpg DALG DF-nl JF-v0`). Release build, fault counts after
checkpoint collapsing:

| Circuit | Collapsed faults | Coverage | Patterns | Undetectable | Runtime |
|---|---|---|---|---|---|
| c17 | 22 | 100.00% | 9 | 0 | 0.01 s |
| c432 | 544 | 98.35% | 73 | 9 | 0.04 s |
| c499 | 594 | 98.65% | 121 | 8 | 0.53 s |
| c880 | 994 | 99.60% | 204 | 4 | 0.97 s |
| c1355 | 1618 | 97.53% | 162 | 40 | 3.16 s |
| c1908 | 2056 | 97.37% | 167 | 54 | 2.66 s |

"Undetectable" means DALG returned no pattern. That bucket mixes genuinely
redundant lines with faults where the search hit the recursion cap in
`dalg.cpp`, so it is an upper bound on real redundancy, not a proof. The cap
exists because c6288 (the 16x16 multiplier) otherwise searches effectively
forever.

### Test point insertion

Where the ATPG numbers above come from deliberate pattern generation, these come
from random patterns — 1000 of them from a 32-bit LFSR, averaged over 5 seeds,
which is the case test points actually exist to fix. Full table in
`results/results.csv`, plots alongside it.

| Circuit | No points | Best | Config | Area | Delay | Gain |
|---|---|---|---|---|---|---|
| c432 | 98.36% | 99.87% | cp, k=4 | +6.9% | +7.7% | +1.51pp |
| c880 | 97.50% | 99.69% | mixed, k=64 | +32.4% | +4.2% | +2.19pp |
| c1355 | 98.12% | 98.31% | mixed, k=64 | +10.5% | +0.0% | +0.20pp |
| c1908 | 94.56% | 95.01% | op, k=64 | +6.3% | +0.0% | +0.45pp |

The gains are not uniform, and that is the actual finding. c880 and c432 respond
well — c880 picks up 2.19 points and c432 lands at 99.87% for under 7% area.
c1355 and c1908 barely move however much budget they get, because what is left
undetected there is not reachable by making one line easier to drive or watch.

Nor is more budget monotonically better. The worst result in the whole sweep is
c1908 with 64 control points at **89.42%**, five points *below* leaving it alone,
and c432 with 64 control points also drops under its baseline. Control points
trade downstream propagation for local controllability, and past some budget the
trade stops paying. Observe points never regress, which is why they are the safe
default when a circuit is unfamiliar.

Cheapest worthwhile insertion, per circuit — coverage gained per percent of area:

| Circuit | Config | Coverage | Area | Gain |
|---|---|---|---|---|
| c432 | cp, k=4 | 99.87% | +6.93% | +1.51pp |
| c880 | op, k=4 | 97.84% | +0.86% | +0.34pp |
| c1355 | cp, k=4 | 98.27% | +2.62% | +0.16pp |
| c1908 | cp, k=4 | 94.91% | +1.58% | +0.35pp |

Plots: `results/coverage_vs_budget.png` and `results/coverage_vs_area.png`.
The second is the one worth looking at — it puts coverage against what it cost
rather than against a budget number, which is the only comparison that decides
anything.


## PPA numbers

Area and delay come from the analytical cell model in `src/ppa.cpp`, scaled off
Nangate 45nm X1 cells. **No synthesis has run** — this machine has no Cadence
licence, so nothing here has seen Genus or Innovus.

The model is a seam, not a stand-in. `-ppa <file>` replaces the cell table with
numbers parsed out of a real Genus report:

```bash
genus> report_area -detail > area.rpt
python3 tools/parse_genus.py area.rpt > ppa_model.txt
# then: TPI 32 out.ckt out.csv -ppa ppa_model.txt
```

The selection code doesn't change — it just gets better numbers, and the report
records which source it used in its `ppa_source` column so results can't be
silently confused. `tools/parse_genus.py` is written against the documented
report layout and tested on a sample in `tests/data/`; it has never run against
a real Genus install.

What the model does not cover: placement, wire load, slew, real critical paths,
and congestion. `wirelength_proxy` is levelised depth standing in for distance
to the boundary — it's called a proxy because that's all it is. Congestion needs
Innovus and isn't addressed.

## Reproducing

```bash
python3 python/sweep.py --circuits c432 c880 c1355 c1908
python3 python/plot.py --results results/results.csv
python3 python/ablation.py --circuit c880
./build/tests/unit_tests
```

## Known limitations

**No Cadence anywhere in the loop.** Modus isn't available either, so there's no
commercial-tool baseline to check the inserter against. The comparison that
would actually validate this is Modus test point insertion on the same circuits,
and it hasn't been run.

**Test pins are counted as primary inputs.** On a real part they'd come off the
scan chain or a test controller, with its own area cost that isn't modelled.

**Coverage is measured on the instrumented netlist**, so the fault population
grows with the test points — the added hardware's own faults are in the
denominator. That's the standard way DFT coverage is reported, but it means
before/after percentages aren't over an identical fault set.

**PODEM still isn't wired into TPG.** It works standalone (`PODEM <node> <val>`)
but the TPG loop only drives DALG, which needs `pathOrientedDecisionMaking_impl`
refactored to return patterns in DALG's format and to support the per-fault
reset TPG does.

**Combinational only** for the ATPG core and BIST. Sequential circuits go
through `scan/`.

**No X-propagation.** Unknowns propagate conservatively, with no X-bounding, so
circuits with many uninitialised paths can undercount detectable faults.

## Inherited from the base simulator

Two bugs I hit while building on it, both fixed here:

- `lev_impl` wrote its report to whatever path was sitting in the command
  arguments, and `scoap`/`pfs`/`podem` all called it internally. Levelising
  during SCOAP would overwrite an unrelated file — in my case a benchmark
  netlist. There's now a `levelize()` that doesn't touch the filesystem.
- `Simulator` was copyable while `Node` owned raw fan-in/fan-out pointers, so a
  copy left both holding freed memory. Copying is deleted now; everything took
  it by reference anyway.
