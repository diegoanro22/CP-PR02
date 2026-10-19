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
    by_key = defaultdict(list)
    with open(os.path.join(folder, "resumen.csv"), newline="") as f:
        for row in csv.DictReader(f):
            by_key[int(row["llave"])].append(row)
    all_n = sorted({int(r["n"]) for rows in by_key.values() for r in rows})

    fig, ax = plt.subplots(figsize=(7, 4.5))
    for key, rows in by_key.items():
        rows.sort(key=lambda r: int(r["n"]))
        ax.errorbar([int(r["n"]) for r in rows],
                    [float(r["promedio"]) for r in rows],
                    yerr=[float(r["desviacion"]) for r in rows],
                    marker="o", capsize=3, label=f"llave {key}")
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
    for key, rows in by_key.items():
        rows = [r for r in rows if r["speedup"]]
        if not rows:
            continue
        ns = [int(r["n"]) for r in rows]
        max_n = max(max_n, *ns)
        ax.plot(ns, [float(r["speedup"]) for r in rows], marker="o", label=f"llave {key}")
    ax.plot([1, max_n], [1, max_n], "k--", alpha=0.5, label="ideal (S = N)")
    ax.set_xticks(all_n)
    ax.set_xlabel("Procesos (N)")
    ax.set_ylabel("Speedup S = T1 / TN")
    ax.set_title("Speedup segun la llave")
    ax.set_yscale("log")
    ax.grid(alpha=0.3, which="both")
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(folder, "speedup.png"), dpi=150)

    print(f"Graficas en {folder}/tiempo.png y {folder}/speedup.png")


if __name__ == "__main__":
    main()
