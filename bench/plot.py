#!/usr/bin/env python3
"""Genera las graficas de tiempo y speedup a partir de resumen.csv.

Cada llave tiene su propio panel; si hay dos repartos se dibujan juntos.
Uso: python bench/plot.py [carpeta]   (por defecto bench/resultados)
"""
import csv
import math
import os
import sys
from collections import defaultdict

import matplotlib.pyplot as plt

ESTILO = {"bloques": ("tab:blue", "-", "o"), "intercalado": ("tab:orange", "--", "s")}


def leer(folder):
    data = defaultdict(lambda: defaultdict(list))  # llave -> modo -> filas
    with open(os.path.join(folder, "resumen.csv"), newline="") as f:
        for row in csv.DictReader(f):
            data[int(row["llave"])][row["modo"]].append(row)
    for modos in data.values():
        for rows in modos.values():
            rows.sort(key=lambda r: int(r["n"]))
    return data


def paneles(data):
    cols = 2 if len(data) > 1 else 1
    rows = math.ceil(len(data) / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(6.2 * cols, 3.8 * rows), squeeze=False)
    flat = axes.flatten()
    for ax in flat[len(data):]:
        ax.axis("off")
    return fig, flat


def usa_log(valores):
    return min(valores) > 0 and max(valores) / min(valores) > 50


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else "bench/resultados"
    data = leer(folder)
    all_n = sorted({int(r["n"]) for modos in data.values() for rows in modos.values() for r in rows})

    # Tiempo promedio con barras de desviacion estandar
    fig, axes = paneles(data)
    for ax, (key, modos) in zip(axes, sorted(data.items())):
        todos = []
        for modo, rows in modos.items():
            color, ls, marker = ESTILO[modo]
            ys = [float(r["promedio"]) for r in rows]
            todos += ys
            ax.errorbar([int(r["n"]) for r in rows], ys, yerr=[float(r["desviacion"]) for r in rows],
                        color=color, linestyle=ls, marker=marker, capsize=3, label=modo)
        if usa_log(todos):
            ax.set_yscale("log")
        else:
            ax.set_ylim(bottom=0)
        ax.set_xticks(all_n)
        ax.set_title(f"llave {key}")
        ax.set_xlabel("Procesos (N)")
        ax.set_ylabel("Tiempo promedio (s)")
        ax.grid(alpha=0.3, which="both")
        ax.legend()
    fig.suptitle("Tiempo de busqueda vs. numero de procesos")
    fig.tight_layout()
    fig.savefig(os.path.join(folder, "tiempo.png"), dpi=150)

    # Speedup S = T1 / TN contra el ideal S = N
    fig, axes = paneles(data)
    for ax, (key, modos) in zip(axes, sorted(data.items())):
        todos = []
        for modo, rows in modos.items():
            rows = [r for r in rows if r["speedup"]]
            color, ls, marker = ESTILO[modo]
            ys = [float(r["speedup"]) for r in rows]
            todos += ys
            ax.plot([int(r["n"]) for r in rows], ys, color=color, linestyle=ls, marker=marker, label=modo)
        ax.plot(all_n, all_n, color="gray", linestyle=":", label="ideal (S = N)")
        if usa_log(todos):
            ax.set_yscale("log")
        else:
            ax.set_ylim(0, max(all_n) + 0.5)
        ax.set_xticks(all_n)
        ax.set_title(f"llave {key}")
        ax.set_xlabel("Procesos (N)")
        ax.set_ylabel("Speedup S = T1 / TN")
        ax.grid(alpha=0.3, which="both")
        ax.legend()
    fig.suptitle("Speedup segun la llave y el reparto")
    fig.tight_layout()
    fig.savefig(os.path.join(folder, "speedup.png"), dpi=150)

    print(f"Graficas en {folder}/tiempo.png y {folder}/speedup.png")


if __name__ == "__main__":
    main()
