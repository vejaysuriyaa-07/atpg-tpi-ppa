# Why the inserter works the way it does

Several things in `src/tpi.cpp` look arbitrary until you see what they were
measured against. The first three sections below are the design choices, the
last two are results worth knowing before reading any coverage number in this
repository. All of it comes from `python/ablation.py` on c880, 1000 patterns,
averaged over 5 LFSR seeds, and can be regenerated with:

    python3 python/ablation.py --circuit c880

## 1. Ranking by detection probability, not SCOAP cost

The obvious move is to reuse the SCOAP numbers the simulator already computes.
They were right there, so that is what the first version did, and it barely
moved the needle -- a few tenths of a point, and at some budgets the coverage
came out *below* the untouched circuit.

The reason is that SCOAP answers the wrong question. Its CC0/CC1/CO are costs:
roughly how much effort a deterministic ATPG run has to spend to control or
observe a line. Random patterns do not spend effort. What decides whether a
random vector catches a fault is a *probability*, and a line can be cheap to
control deliberately while still being very unlikely to take the needed value
by chance.

COP computes that probability directly. One forward pass for P(line = 1)
assuming unbiased inputs, one backward pass for the odds a flip on the line
survives to an output, and the product of the two is the chance a random
pattern detects the fault. Rank by the worse of the two stuck-at directions and
the budget goes to the lines that are actually being missed.

Head to head on c880, mixed mode, weight 3, baseline 97.50%:

| budget | COP | SCOAP |
|---|---|---|
| 8 | 97.13 +/-0.65 | 97.80 +/-0.46 |
| 16 | **98.40** +/-0.40 | 98.05 +/-0.45 |
| 32 | **99.26** +/-0.20 | 98.33 +/-0.51 |
| 64 | **99.35** +/-0.21 | 98.68 +/-0.40 |

At a budget of 8 SCOAP is actually ahead, and both sit within noise of doing
nothing. The separation opens up once there are enough points to matter, and by
32 it is nearly a point. `-metric scoap` keeps the old ranking so this can be re-run.

## 2. Control points need a weighted enable

A control point splices a gate between a line and everything it drives, with a
test pin on the side input: AND to force 0, OR to force 1. The first version
drove that pin straight off the pattern generator.

That makes it worse than doing nothing. An unbiased pin means the AND gate
holds its output at 0 half the time, so half of every random pattern's work on
the downstream cone is thrown away. The node itself gets easier to control and
everything behind it gets harder to test, and on c880 the net effect was a
*loss* against the baseline.

Gating the enable through a small tree of `weight` pins -- OR for control-0, AND
for control-1 -- drops the activation rate to 2^-weight. c880, 32 control points,
baseline 97.50%:

| weight | fires | coverage |
|---|---|---|
| 1 | 1/2 | 94.18 +/-0.53 — *worse than no points* |
| 2 | 1/4 | 97.92 +/-0.65 |
| 3 | 1/8 | **98.94** +/-0.21 |
| 4 | 1/16 | 98.34 +/-0.49 |
| 5 | 1/32 | 97.61 +/-0.36 |

It is a proper curve with a peak. Too eager and the point spends its time
blocking real patterns, which is bad enough at weight 1 to lose three points
against doing nothing at all; too timid and it stops firing when it is needed
and the gain decays back toward the baseline. One in eight is the default.

## 3. The seeds have to be spread out

Worth knowing before reading any BIST coverage number, including the ones in the
README:

| seed | set bits | coverage |
|---|---|---|
| 1 | 1 | **14.26%** |
| 3 | 2 | **14.26%** |
| 255 | 8 | 96.76% |
| 2654435761 | 19 | 98.30% |
| 1013904226 | 18 | 96.48% |
| 3668339987 | 17 | 97.50% |

It is only the very sparse seeds that fall over -- 255 has eight set bits and is
perfectly fine. But seed 1 loses 83 points of coverage against seed 255 on the
same circuit with the same pattern count.

Nothing is wrong with the LFSR. A seed of 1 is a single set bit, and c880 has 60
inputs against a 32-bit LFSR, so the early patterns are nearly all zeros and
stay correlated for a long time. 1000 patterns is not enough to recover.

`python/sweep.py` builds its seeds from Knuth's multiplicative constant, which
gives well-mixed 32-bit values and keeps runs reproducible. Any single-seed
coverage number is worth about a percent of noise, which is the same size as the
effect being measured -- hence 5 seeds everywhere, with the spread reported.

## 4. Observe points and control points are not interchangeable

They fail differently, which is why `-mode` exists.

An observe point cannot hurt. It taps a line out to a new output and changes
nothing about how the circuit computes, so coverage only goes up. It is also
cheap: a branch and an output pin, no new logic in any existing path.

A control point can hurt, as above, and costs more -- the weighted enable tree
means `weight` new input pins and two gates per point. Against that, it is the
only thing that fixes a line random patterns cannot drive at all.

Which one wins depends on the circuit, and the sweep shows both cases. On c880
observe points give the best coverage per unit of area (+0.34pp for 0.86%); on
c432 control points do (+1.51pp for 6.93%). The sweep also has the failure mode:
c1908 with 64 control points lands at 90.45% against a 94.56% baseline, four
points *down*. No observe-point configuration in the sweep ever regressed.
`-mode mixed` decides per node by comparing the two probabilities, with a `-bias`
factor tilting the comparison toward observe points to pay for the asymmetry
above.

## What is not modelled

The area and delay numbers come from the analytical cell model in `src/ppa.cpp`,
scaled off Nangate 45nm X1 cells. No synthesis has run. In particular:

- Area counts cells, not placement, so it ignores everything a floorplan does.
- Delay is a longest-path sum over a static per-cell table. No wire load, no
  slew, no real critical path.
- `wirelength_proxy` is levelised depth standing in for distance to the
  boundary. It is a proxy in the name because that is all it is.
- The test pins are counted as primary inputs. On a real part they would come
  off the scan chain or a dedicated test controller, which has its own cost.

`PpaModel::loadFromFile` replaces the cell table with numbers parsed from a real
Genus report (`tools/parse_genus.py`), which fixes the first two. Congestion and
wirelength need Innovus and are not addressed here.

## 5. Determinism across toolchains

Worth recording because it was not obvious. The same command on the same circuit
selected a different set of test points on macOS than on Linux -- the same
*number* of points and the same area, but different lines, and coverage a few
tenths apart.

Nothing random is involved. Lines tie on detection probability constantly;
symmetric logic hands whole groups the same value. `std::sort` is not stable, so
within a tied group the winners came down to whether libc++ or libstdc++ was
doing the sorting. Adding the line number as a second sort key pins the order
everywhere, and the two platforms now produce byte-identical netlists.

The numbers in this file and in the README were regenerated after that fix. They
are what the current code produces; the earlier ones were what an unstable sort
happened to pick on one machine.
