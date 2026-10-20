#!/usr/bin/env python3
"""Genera las graficas de tiempo y speedup a partir de resumen.csv.

Uso: python bench/plot.py [carpeta]   (por defecto bench/resultados)
"""
import csv
import os
import sys
from collections import defaultdict

import matplotlib.pyplot as plt


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else "bench/resultados"
    series = defaultdict(list)  # (llave, modo) -> filas
    with open(os.path.join(folder, "resumen.csv"), newline="") as f:
        for row in csv.DictReader(f):
            series[(int(row["llave"]), row["modo"])].append(row)
    all_n = sorted({int(r["n"]) for rows in series.values() for r in rows})
    several_modes = len({modo for _, modo in series}) > 1

    def label(key, modo):
        return f"llave {key} ({modo})" if several_modes else f"llave {key}"

    def style(modo):
        return "--" if modo == "intercalado" else "-"

    fig, ax = plt.subplots(figsize=(7, 4.5))
    for (key, modo), rows in series.items():
        rows.sort(key=lambda r: int(r["n"]))
        ax.errorbar([int(r["n"]) for r in rows],
                    [float(r["promedio"]) for r in rows],
                    yerr=[float(r["desviacion"]) for r in rows],
                    marker="o", capsize=3, linestyle=style(modo), label=label(key, modo))
    ax.set_xticks(all_n)
    ax.set_xlabel("Procesos (N)")
    ax.set_ylabel("Tiempo promedio (s)")
    ax.set_title("Tiempo de busqueda vs. numero de procesos")
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(folder, "tiempo.png"), dpi=150)

    fig, ax = plt.subplots(figsize=(7, 4.5))
    max_n = 1
    for (key, modo), rows in series.items():
        rows = [r for r in rows if r["speedup"]]
        if not rows:
            continue
        ns = [int(r["n"]) for r in rows]
        max_n = max(max_n, *ns)
        ax.plot(ns, [float(r["speedup"]) for r in rows], marker="o",
                linestyle=style(modo), label=label(key, modo))
    ax.plot([1, max_n], [1, max_n], "k:", alpha=0.5, label="ideal (S = N)")
    ax.set_xticks(all_n)
    ax.set_xlabel("Procesos (N)")
    ax.set_ylabel("Speedup S = T1 / TN")
    ax.set_title("Speedup segun la llave y el reparto" if several_modes else "Speedup segun la llave")
    ax.set_yscale("log")
    ax.grid(alpha=0.3, which="both")
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(folder, "speedup.png"), dpi=150)

    print(f"Graficas en {folder}/tiempo.png y {folder}/speedup.png")


if __name__ == "__main__":
    main()
