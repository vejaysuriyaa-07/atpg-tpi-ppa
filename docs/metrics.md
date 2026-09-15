# Why the inserter works the way it does

Three things in `src/tpi.cpp` look arbitrary until you see what they were
measured against. All the numbers here come from `python/ablation.py` on c880,
1000 patterns, averaged over 5 LFSR seeds, and can be regenerated with:

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
| 16 | **98.79** +/-0.32 | 98.05 +/-0.45 |
| 32 | **99.63** +/-0.05 | 98.33 +/-0.51 |
| 64 | **99.69** +/-0.10 | 98.68 +/-0.40 |

At a budget of 8 SCOAP is actually ahead, and both sit within noise of doing
nothing. The separation opens up once there are enough points to matter, and by
32 it is 1.3 points. `-metric scoap` keeps the old ranking so this can be re-run.

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
| 1 | 1/2 | 94.52 +/-0.47 — *worse than no points* |
| 2 | 1/4 | 98.10 +/-0.44 |
| 3 | 1/8 | **98.58** +/-0.22 |
| 4 | 1/16 | 98.03 +/-0.30 |
| 5 | 1/32 | 97.10 +/-0.37 — *worse than no points* |

It is a proper curve with a peak, and both ends of it lose to the untouched
circuit. Too eager and the point spends its time blocking real patterns; too
timid and it never fires when it is needed. One in eight is the default.

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
c1908 with 64 control points lands at 89.42% against a 94.56% baseline, over five
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
