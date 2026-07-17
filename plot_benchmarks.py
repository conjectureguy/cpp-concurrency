#!/usr/bin/env python3
"""Generate simple benchmark comparison plots from a CSV file.

Usage:
    python plot_benchmarks.py benchmark_results/with_memory_ordering.csv

Plots are written to:
    benchmark_plots/<csv-file-stem>/

Each plot fixes one workload/capacity/metric and compares implementations across item counts.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
import tempfile
from pathlib import Path


def require_plotting_modules():
    os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "matplotlib-cache"))
    try:
        import matplotlib.pyplot as plt
        import pandas as pd
    except ImportError as exc:
        missing = exc.name or "required plotting dependency"
        raise SystemExit(
            f"Missing dependency: {missing}. Install pandas and matplotlib, then rerun."
        ) from exc

    return pd, plt


def slugify(value: str) -> str:
    value = value.strip().lower()
    value = re.sub(r"[^a-z0-9]+", "_", value)
    return value.strip("_") or "plot"


def friendly_metric_name(metric: str) -> str:
    names = {
        "seconds": "Runtime (seconds)",
        "mitems_per_second": "Throughput (million items/sec)",
        "ns_per_item": "Nanoseconds per item",
        "p50_ns": "P50 latency (ns)",
        "p95_ns": "P95 latency (ns)",
        "p99_ns": "P99 latency (ns)",
    }
    return names.get(metric, metric.replace("_", " ").title())


def format_int_label(value) -> str:
    try:
        if value != value:
            return "NA"
        return f"{int(value):,}"
    except (TypeError, ValueError):
        return str(value)


def prepare_dataframe(pd, csv_path: Path):
    df = pd.read_csv(csv_path)
    if df.empty:
        raise SystemExit(f"No rows found in {csv_path}")
    if "benchmark" not in df.columns:
        raise SystemExit("CSV must contain a 'benchmark' column.")

    numeric_candidates = [
        "capacity",
        "items",
        "seconds",
        "mitems_per_second",
        "ns_per_item",
        "p50_ns",
        "p95_ns",
        "p99_ns",
        "validation_passed",
    ]
    for column in numeric_candidates:
        if column in df.columns:
            df[column] = pd.to_numeric(df[column], errors="coerce")

    if "capacity" in df.columns and "items" in df.columns:
        df["config"] = (
            "cap="
            + df["capacity"].map(format_int_label)
            + ", items="
            + df["items"].map(format_int_label)
        )
    else:
        df["config"] = "all"

    if df["benchmark"].astype(str).str.contains("/", regex=False).any():
        split = df["benchmark"].astype(str).str.split("/", n=1, expand=True)
        df["implementation"] = split[0]
        df["workload"] = split[1].fillna("benchmark")
    else:
        df["implementation"] = df["benchmark"].astype(str)
        df["workload"] = "benchmark"

    return df


def comparison_group_columns(df) -> list[str]:
    columns = ["workload"]
    if "capacity" in df.columns:
        columns.append("capacity")
    return columns


def group_title_parts(group_key, columns: list[str]) -> dict[str, str]:
    values = group_key if isinstance(group_key, tuple) else (group_key,)
    return {column: format_int_label(value) for column, value in zip(columns, values)}


def save_implementation_comparison(df, plt, out_dir: Path, metric: str) -> list[Path]:
    if metric not in df.columns or df[metric].dropna().empty:
        return []

    output_paths: list[Path] = []
    columns = comparison_group_columns(df)

    for group_key, group in df.groupby(columns, sort=True):
        if "items" in group.columns:
            plot_df = group.pivot_table(
                index="implementation",
                columns="items",
                values=metric,
                aggfunc="mean",
                sort=True,
            )
            column_labels = [format_int_label(column) for column in plot_df.columns]
            plot_df.columns = column_labels
            item_label = "Item count"
        else:
            plot_df = group.pivot_table(
                index="implementation",
                values=metric,
                aggfunc="mean",
                sort=True,
            )
            item_label = None

        plot_df = plot_df.dropna(how="all")
        if plot_df.empty:
            continue
        sort_column = plot_df.columns[-1] if hasattr(plot_df, "columns") else metric
        plot_df = plot_df.sort_values(by=sort_column, ascending=False)

        parts = group_title_parts(group_key, columns)
        workload = parts.get("workload", "benchmark")
        capacity = parts.get("capacity")

        fig_width = max(8.5, len(plot_df.index) * 1.55)
        _, ax = plt.subplots(figsize=(fig_width, 5.6))
        colors = ["#4c78a8", "#f58518", "#54a24b", "#e45756", "#72b7b2", "#b279a2"]
        plot_df.plot(kind="bar", ax=ax, color=colors[: len(plot_df.columns)], width=0.74)

        title = f"{friendly_metric_name(metric)}: {workload}"
        subtitle_parts = []
        if capacity is not None:
            subtitle_parts.append(f"capacity={capacity}")
        if subtitle_parts:
            title += f" ({', '.join(subtitle_parts)})"

        ax.set_title(title)
        ax.set_xlabel("Implementation")
        ax.set_ylabel(friendly_metric_name(metric))
        ax.grid(axis="y", alpha=0.3)
        ax.set_axisbelow(True)
        if item_label is not None:
            ax.legend(title=item_label)
        plt.xticks(rotation=20, ha="right")

        max_value = plot_df.max().max()
        for container in ax.containers:
            ax.bar_label(container, fmt="%.3g", fontsize=8, padding=2)
        if max_value > 0:
            ax.set_ylim(top=max_value * 1.18)

        plt.tight_layout()

        filename_parts = [slugify(str(workload))]
        if capacity is not None:
            filename_parts.append(f"capacity_{slugify(capacity)}")
        filename_parts.append(slugify(metric))

        output_path = out_dir / ("__".join(filename_parts) + ".png")
        plt.savefig(output_path, dpi=160)
        plt.close()
        output_paths.append(output_path)

    return output_paths


def save_latency_percentile_comparison(df, plt, out_dir: Path) -> list[Path]:
    percentile_metrics = ["p50_ns", "p95_ns", "p99_ns"]
    available_metrics = [
        metric for metric in percentile_metrics
        if metric in df.columns and not df[metric].dropna().empty
    ]
    if not available_metrics:
        return []

    output_paths: list[Path] = []
    columns = comparison_group_columns(df)
    colors = ["#4c78a8", "#f58518", "#54a24b", "#e45756", "#72b7b2", "#b279a2"]

    for group_key, group in df.groupby(columns, sort=True):
        label_columns = ["implementation"]
        if "items" in group.columns:
            label_columns.append("items")

        melted = group.melt(
            id_vars=label_columns,
            value_vars=available_metrics,
            var_name="percentile",
            value_name="latency_ns",
        ).dropna(subset=["latency_ns"])
        if melted.empty:
            continue

        if "items" in melted.columns:
            melted["series"] = (
                melted["items"].map(format_int_label)
                + " "
                + melted["percentile"].str.replace("_ns", "", regex=False).str.upper()
            )
        else:
            melted["series"] = melted["percentile"].str.replace("_ns", "", regex=False).str.upper()

        plot_df = melted.pivot_table(
            index="implementation",
            columns="series",
            values="latency_ns",
            aggfunc="mean",
            sort=False,
        ).dropna(how="all")
        if plot_df.empty:
            continue

        sort_column = plot_df.columns[-1]
        plot_df = plot_df.sort_values(by=sort_column, ascending=False)

        parts = group_title_parts(group_key, columns)
        workload = parts.get("workload", "benchmark")
        capacity = parts.get("capacity")

        fig_width = max(9.5, len(plot_df.index) * len(plot_df.columns) * 0.42)
        _, ax = plt.subplots(figsize=(fig_width, 5.8))
        plot_df.plot(kind="bar", ax=ax, color=colors[: len(plot_df.columns)], width=0.78)

        title = f"Latency percentiles (ns): {workload}"
        subtitle_parts = []
        if capacity is not None:
            subtitle_parts.append(f"capacity={capacity}")
        if subtitle_parts:
            title += f" ({', '.join(subtitle_parts)})"

        ax.set_title(title)
        ax.set_xlabel("Implementation")
        ax.set_ylabel("Latency (ns)")
        ax.grid(axis="y", alpha=0.3)
        ax.set_axisbelow(True)
        ax.legend(title="Item count / percentile")
        plt.xticks(rotation=20, ha="right")

        max_value = plot_df.max().max()
        for container in ax.containers:
            ax.bar_label(container, fmt="%.3g", fontsize=8, padding=2)
        if max_value > 0:
            ax.set_ylim(top=max_value * 1.18)

        plt.tight_layout()

        filename_parts = [slugify(str(workload))]
        if capacity is not None:
            filename_parts.append(f"capacity_{slugify(capacity)}")
        filename_parts.append("latency_percentiles_ns")

        output_path = out_dir / ("__".join(filename_parts) + ".png")
        plt.savefig(output_path, dpi=160)
        plt.close()
        output_paths.append(output_path)

    return output_paths


def generate_plots(csv_path: Path, output_root: Path) -> list[Path]:
    pd, plt = require_plotting_modules()
    df = prepare_dataframe(pd, csv_path)

    out_dir = output_root / csv_path.stem
    out_dir.mkdir(parents=True, exist_ok=True)
    for old_plot in out_dir.glob("*.png"):
        old_plot.unlink()

    generated: list[Path] = []
    metrics = [
        "seconds",
        "mitems_per_second",
        "ns_per_item",
    ]

    for metric in metrics:
        generated.extend(save_implementation_comparison(df, plt, out_dir, metric))
    generated.extend(save_latency_percentile_comparison(df, plt, out_dir))

    if not generated:
        raise SystemExit("No plottable metric columns were found.")

    return generated


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate plots for benchmark CSV results.")
    parser.add_argument("csv_file", type=Path, help="Path to benchmark CSV file.")
    parser.add_argument(
        "-o",
        "--output-root",
        type=Path,
        default=Path("benchmark_plots"),
        help="Root folder for generated plot folders. Default: benchmark_plots",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    csv_path = args.csv_file
    if not csv_path.exists():
        print(f"CSV file not found: {csv_path}", file=sys.stderr)
        return 1
    if not csv_path.is_file():
        print(f"CSV path is not a file: {csv_path}", file=sys.stderr)
        return 1

    generated = generate_plots(csv_path, args.output_root)
    print(f"Generated {len(generated)} plot(s) in {generated[0].parent}")
    for path in generated:
        print(f"  {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
