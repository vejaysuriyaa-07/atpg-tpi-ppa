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

`-metric scoap` still selects points that way, so the comparison can be re-run.

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
for control-1 -- drops the activation rate to 2^-weight. At weight 3 the point
fires one time in eight, which is often enough to break the stuck line and rare
enough to leave normal propagation intact. That is the default. Past about 4 the
point stops firing often enough to matter and the gain flattens out.

## 3. The seeds have to be spread out

Worth knowing before reading any BIST coverage number, including the ones in the
README: on c880, seed 1 gives **14%** coverage where a well-mixed seed gives
**97%**.

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
observe points give more coverage per unit of area; on c432 control points do.
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
