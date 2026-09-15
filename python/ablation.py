#!/usr/bin/env python3
"""The two design decisions in the inserter, measured rather than asserted.

  1. Ranking nodes by COP detection probability instead of SCOAP cost.
  2. Gating a control point's enable so it fires 2^-weight of the time
     instead of half the time.

Both were picked because the numbers below said so, not the other way round.

  python3 python/ablation.py --circuit c880
"""

import argparse
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SIM = ROOT / "build" / "simulator"
BIST = ROOT / "bist" / "build" / "bist-simulator"

SEEDS = [(2654435761 * (i + 1)) & 0xFFFFFFFF for i in range(5)]


def coverage(netlist, patterns, seed):
    out = subprocess.run([str(BIST), str(netlist), "-n", str(patterns),
                          "-seed", str(seed)],
                         capture_output=True, text=True, check=True).stdout
    for line in out.splitlines():
        if "Fault Coverage" in line:
            return float(line.split()[-1].rstrip("%"))
    raise RuntimeError(f"no coverage for {netlist}")


def build(circuit, out, rep, spec):
    """spec is a full TPI argument string with @OUT@/@REP@ placeholders."""
    args = spec.replace("@OUT@", str(out)).replace("@REP@", str(rep))
    script = f"READ {circuit}\nTPI {args}\nQUIT\n"
    subprocess.run([str(SIM)], input=script, capture_output=True, text=True, cwd=ROOT)


def measure(circuit, work, spec, patterns):
    out = work / "ablate.ckt"
    rep = work / "ablate.csv"
    for f in (out, rep):
        if f.exists():
            f.unlink()
    build(circuit, out, rep, spec)
    if not out.exists():
        return None
    covs = [coverage(out, patterns, s) for s in SEEDS]
    return statistics.mean(covs), statistics.pstdev(covs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--circuit", default="c880")
    ap.add_argument("--budgets", nargs="+", type=int, default=[8, 16, 32, 64])
    ap.add_argument("--weights", nargs="+", type=int, default=[1, 2, 3, 4, 5])
    ap.add_argument("--patterns", type=int, default=1000)
    ap.add_argument("--out", default="results")
    args = ap.parse_args()

    ckt = ROOT / "ckts" / f"{args.circuit}.ckt"
    if not ckt.exists():
        sys.exit(f"{ckt} not found")
    work = ROOT / args.out / "ablation"
    work.mkdir(parents=True, exist_ok=True)

    base = [coverage(ckt, args.patterns, s) for s in SEEDS]
    base_mean = statistics.mean(base)
    print(f"{args.circuit}, {args.patterns} patterns, {len(SEEDS)} seeds")
    print(f"no test points: {base_mean:.2f}% "
          f"+/-{statistics.pstdev(base):.2f}\n")

    print("ranking metric (mixed mode, weight 3)")
    print(f"  {'budget':>6}  {'COP':>14}  {'SCOAP':>14}")
    rows = []
    for k in args.budgets:
        line = f"  {k:>6}"
        cell = {}
        for metric in ("cop", "scoap"):
            r = measure(ckt, work,
                        f"{k} @OUT@ @REP@ -mode mixed -weight 3 -metric {metric}",
                        args.patterns)
            cell[metric] = r
            line += f"  {r[0]:>8.2f} +/-{r[1]:.2f}" if r else f"  {'n/a':>14}"
        print(line)
        rows.append((k, cell))

    print("\ncontrol point enable weighting (cp mode, COP ranking)")
    print(f"  {'weight':>6}  {'activation':>10}  {'coverage':>16}")
    for w in args.weights:
        r = measure(ckt, work,
                    f"32 @OUT@ @REP@ -mode cp -metric cop -weight {w}",
                    args.patterns)
        act = f"1/{2 ** w}"
        if r:
            flag = "  <-- worse than no points" if r[0] < base_mean else ""
            print(f"  {w:>6}  {act:>10}  {r[0]:>8.2f} +/-{r[1]:.2f}{flag}")
        else:
            print(f"  {w:>6}  {act:>10}  {'n/a':>16}")

    print("\nLFSR seed sensitivity (no test points)")
    for s in [1, 3, 255] + SEEDS[:3]:
        c = coverage(ckt, args.patterns, s)
        bits = bin(s).count("1")
        note = "  <-- low Hamming weight seed" if bits <= 8 else ""
        print(f"  seed {s:<12} popcount {bits:>2}  {c:>6.2f}%{note}")


if __name__ == "__main__":
    main()
