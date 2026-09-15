#!/usr/bin/env python3
"""Sweep test point budgets and record what each one buys.

For every (circuit, mode, budget) this inserts the points, runs the BIST fault
simulator over several LFSR seeds, and pairs the resulting coverage with the
area and timing numbers the inserter reported. Several seeds because a single
LFSR run moves around by a percent or so on its own, which is the same order as
the effect being measured.

  python3 python/sweep.py --circuits c880 c1355 --budgets 0 8 16 32
"""

import argparse
import csv
import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SIM = ROOT / "build" / "simulator"
BIST = ROOT / "bist" / "build" / "bist-simulator"

COVERAGE_KEY = "Fault Coverage:"
FAULTS_KEY = "Total Faults:"
DETECTED_KEY = "Detected Faults:"


def run_bist(netlist, patterns, seed):
    """Returns (coverage_pct, total_faults, detected_faults)."""
    out = subprocess.run(
        [str(BIST), str(netlist), "-n", str(patterns), "-seed", str(seed)],
        capture_output=True, text=True, check=True).stdout

    cov = total = detected = None
    for line in out.splitlines():
        line = line.strip()
        if line.startswith(COVERAGE_KEY):
            cov = float(line.split()[-1].rstrip("%"))
        elif line.startswith(FAULTS_KEY):
            total = int(line.split()[-1])
        elif line.startswith(DETECTED_KEY):
            detected = int(line.split()[-1])
    if cov is None:
        raise RuntimeError(f"no coverage in bist output for {netlist}")
    return cov, total, detected


def insert_points(circuit, budget, netlist, report, mode, metric, weight, bias):
    script = (
        f"READ {circuit}\n"
        f"TPI {budget} {netlist} {report} -mode {mode} -metric {metric} "
        f"-weight {weight} -bias {bias}\n"
        "QUIT\n"
    )
    res = subprocess.run([str(SIM)], input=script, capture_output=True,
                         text=True, cwd=ROOT)
    if not Path(netlist).exists():
        raise RuntimeError(f"TPI produced no netlist for {circuit} k={budget}:\n"
                           f"{res.stdout}\n{res.stderr}")


def read_report(path):
    with open(path) as f:
        return next(csv.DictReader(f))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--circuits", nargs="+", default=["c432", "c880", "c1355"])
    ap.add_argument("--budgets", nargs="+", type=int, default=[0, 4, 8, 16, 32])
    ap.add_argument("--modes", nargs="+", default=["mixed", "op", "cp"])
    ap.add_argument("--metric", default="cop", choices=["cop", "scoap"])
    ap.add_argument("--weight", type=int, default=3)
    ap.add_argument("--bias", type=float, default=4.0)
    ap.add_argument("--patterns", type=int, default=1000)
    ap.add_argument("--seeds", type=int, default=5)
    ap.add_argument("--out", default="results")
    args = ap.parse_args()

    for tool in (SIM, BIST):
        if not tool.exists():
            sys.exit(f"missing {tool} -- build it first (see README)")

    outdir = ROOT / args.out
    (outdir / "netlists").mkdir(parents=True, exist_ok=True)
    # Seeds have to be spread across the state space. A low Hamming weight
    # start like 1 leaves the LFSR shifting a single bit through 60 inputs for
    # hundreds of cycles, and c880 lands at 14% coverage instead of 96%. Knuth's
    # multiplicative constant gives well mixed values and keeps the run
    # reproducible.
    seeds = [(2654435761 * (i + 1)) & 0xFFFFFFFF for i in range(args.seeds)]
    rows = []

    for circuit in args.circuits:
        ckt = ROOT / "ckts" / f"{circuit}.ckt"
        if not ckt.exists():
            print(f"skipping {circuit}: {ckt} not found")
            continue

        for mode in args.modes:
            for budget in args.budgets:
                if budget == 0:
                    netlist, rep = ckt, None
                else:
                    tag = f"{circuit}_{mode}_{budget}"
                    netlist = outdir / "netlists" / f"{tag}.ckt"
                    rep = outdir / "netlists" / f"{tag}.csv"
                    insert_points(ckt, budget, netlist, rep, mode,
                                  args.metric, args.weight, args.bias)

                covs = []
                total = detected = 0
                for s in seeds:
                    c, t, d = run_bist(netlist, args.patterns, s)
                    covs.append(c)
                    total, detected = t, d

                row = {
                    "circuit": circuit,
                    "mode": mode,
                    "metric": args.metric,
                    "budget": budget,
                    "coverage_mean": round(statistics.mean(covs), 3),
                    "coverage_std": round(statistics.pstdev(covs), 3) if len(covs) > 1 else 0.0,
                    "coverage_min": min(covs),
                    "coverage_max": max(covs),
                    "total_faults": total,
                    "detected_faults": detected,
                    "seeds": len(covs),
                    "patterns": args.patterns,
                }

                if rep is not None:
                    r = read_report(rep)
                    row.update({
                        "control_points": int(r["control_points"]),
                        "observe_points": int(r["observe_points"]),
                        "added_pis": int(r["added_pis"]),
                        "added_pos": int(r["added_pos"]),
                        "area_pct": float(r["area_pct"]),
                        "delay_pct": float(r["delay_pct"]),
                        "wirelength_proxy": float(r["wirelength_proxy"]),
                        "ppa_source": r["ppa_source"],
                    })
                else:
                    row.update({
                        "control_points": 0, "observe_points": 0,
                        "added_pis": 0, "added_pos": 0,
                        "area_pct": 0.0, "delay_pct": 0.0,
                        "wirelength_proxy": 0.0, "ppa_source": "baseline",
                    })

                rows.append(row)
                print(f"{circuit:8} {mode:6} k={budget:<3} "
                      f"cov={row['coverage_mean']:6.2f}% "
                      f"+/-{row['coverage_std']:.2f}  "
                      f"area+{row['area_pct']:.2f}%")

                # A budget of zero is the untouched circuit, identical for
                # every mode, so measure it once and reuse it.
                if budget == 0 and mode != args.modes[0]:
                    rows[-1]["mode"] = mode

    csv_path = outdir / "results.csv"
    with open(csv_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print(f"\nwrote {csv_path} ({len(rows)} rows)")


if __name__ == "__main__":
    main()
