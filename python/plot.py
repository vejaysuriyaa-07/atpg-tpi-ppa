#!/usr/bin/env python3
"""Plots from a sweep.py results.csv.

Two views. The first is the obvious one -- coverage against how many points
were spent. The second is the one that actually decides anything: coverage
against the area it cost, which is where the two kinds of test point separate.

  python3 python/plot.py --results results/results.csv
"""

import argparse
from pathlib import Path

import matplotlib
matplotlib.use("Agg")  # no display in a container
import matplotlib.pyplot as plt
import pandas as pd

MODE_STYLE = {
    "op":    ("#0072B2", "o", "observe only"),
    "cp":    ("#D55E00", "s", "control only"),
    "mixed": ("#009E73", "^", "mixed"),
}


def coverage_vs_budget(df, outdir):
    circuits = sorted(df.circuit.unique())
    fig, axes = plt.subplots(1, len(circuits), figsize=(4.2 * len(circuits), 3.8),
                             squeeze=False, sharey=False)

    for ax, circuit in zip(axes[0], circuits):
        sub = df[df.circuit == circuit]
        base = sub[sub.budget == 0].coverage_mean.mean()
        for mode, (color, marker, label) in MODE_STYLE.items():
            m = sub[sub["mode"] == mode].sort_values("budget")
            if m.empty:
                continue
            ax.errorbar(m.budget, m.coverage_mean, yerr=m.coverage_std,
                        color=color, marker=marker, capsize=3, label=label,
                        linewidth=1.6, markersize=5)
        ax.axhline(base, color="#666666", linestyle="--", linewidth=1,
                   label="no test points")
        ax.set_title(circuit)
        ax.set_xlabel("test points")
        ax.grid(alpha=0.25)
    axes[0][0].set_ylabel("stuck-at coverage (%)")
    axes[0][-1].legend(fontsize=8, loc="lower right")
    fig.suptitle("Random-pattern coverage vs test point budget "
                 "(1000 patterns, mean of 5 LFSR seeds)", fontsize=11)
    fig.tight_layout()
    path = outdir / "coverage_vs_budget.png"
    fig.savefig(path, dpi=150)
    print(f"wrote {path}")


def coverage_vs_area(df, outdir):
    """The PPA view: what each percent of extra silicon actually bought."""
    circuits = sorted(df.circuit.unique())
    fig, axes = plt.subplots(1, len(circuits), figsize=(4.2 * len(circuits), 3.8),
                             squeeze=False)

    for ax, circuit in zip(axes[0], circuits):
        sub = df[df.circuit == circuit]
        base = sub[sub.budget == 0].coverage_mean.mean()
        for mode, (color, marker, label) in MODE_STYLE.items():
            m = sub[(sub["mode"] == mode) & (sub.budget > 0)].sort_values("area_pct")
            if m.empty:
                continue
            ax.plot(m.area_pct, m.coverage_mean - base, color=color,
                    marker=marker, label=label, linewidth=1.6, markersize=5)
        ax.axhline(0, color="#666666", linestyle="--", linewidth=1)
        ax.set_title(circuit)
        ax.set_xlabel("area overhead (%)")
        ax.grid(alpha=0.25)
    axes[0][0].set_ylabel("coverage gained (pp)")
    axes[0][-1].legend(fontsize=8, loc="lower right")
    fig.suptitle("What the area bought (analytical PPA model)", fontsize=11)
    fig.tight_layout()
    path = outdir / "coverage_vs_area.png"
    fig.savefig(path, dpi=150)
    print(f"wrote {path}")


def efficiency_table(df, outdir):
    """Coverage gained per percent of area -- the number worth arguing over."""
    rows = []
    for circuit in sorted(df.circuit.unique()):
        sub = df[df.circuit == circuit]
        base = sub[sub.budget == 0].coverage_mean.mean()
        for mode in MODE_STYLE:
            m = sub[(sub["mode"] == mode) & (sub.budget > 0)]
            for _, r in m.iterrows():
                gain = r.coverage_mean - base
                rows.append({
                    "circuit": circuit,
                    "mode": mode,
                    "budget": int(r.budget),
                    "coverage": r.coverage_mean,
                    "gain_pp": round(gain, 3),
                    "area_pct": r.area_pct,
                    "delay_pct": r.delay_pct,
                    "pp_per_area_pct": round(gain / r.area_pct, 4) if r.area_pct else 0.0,
                })
    out = pd.DataFrame(rows).sort_values(
        ["circuit", "pp_per_area_pct"], ascending=[True, False])
    path = outdir / "efficiency.csv"
    out.to_csv(path, index=False)
    print(f"wrote {path}")

    print("\nbest coverage per percent of area, by circuit:")
    for circuit in sorted(out.circuit.unique()):
        top = out[out.circuit == circuit].head(3)
        print(f"\n  {circuit}")
        for _, r in top.iterrows():
            print(f"    {r['mode']:6} k={int(r.budget):<3} "
                  f"+{r.gain_pp:5.2f}pp for {r.area_pct:5.2f}% area "
                  f"-> {r.pp_per_area_pct:.3f} pp per area%")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="results/results.csv")
    ap.add_argument("--outdir", default=None)
    args = ap.parse_args()

    path = Path(args.results)
    if not path.exists():
        raise SystemExit(f"{path} not found -- run python/sweep.py first")
    df = pd.read_csv(path)
    outdir = Path(args.outdir) if args.outdir else path.parent
    outdir.mkdir(parents=True, exist_ok=True)

    coverage_vs_budget(df, outdir)
    coverage_vs_area(df, outdir)
    efficiency_table(df, outdir)


if __name__ == "__main__":
    main()
