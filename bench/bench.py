#!/usr/bin/env python3
"""Mide el tiempo de bruteforce con varias llaves y numeros de procesos.

Cifra el texto con cada llave, corre cada caso varias veces y guarda:
  - crudo.csv:   una fila por ejecucion
  - resumen.csv: promedio, desviacion, speedup y eficiencia por (llave, N)

Se corre en Linux/WSL desde la raiz del repo, por ejemplo:
  python3 bench/bench.py --upper 1048576 --keys 123456 524289 --procs 1 2 3 4 --reps 5
"""
import argparse
import csv
import os
import statistics
import subprocess
import sys
import tempfile


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--bin", default="./bruteforce", help="ejecutable MPI")
    p.add_argument("--encrypt", default="des/encrypt_file", help="programa que cifra el texto")
    p.add_argument("--text", default="des/texto_350_palabras.txt", help="texto plano")
    p.add_argument("--phrase", default=" los mismos del proyecto 1 ", help="frase clave")
    p.add_argument("--upper", type=int, required=True, help="se buscan llaves en [0, upper)")
    p.add_argument("--keys", type=int, nargs="+", required=True, help="llaves a probar")
    p.add_argument("--procs", type=int, nargs="+", default=[1, 2, 3, 4], help="valores de N")
    p.add_argument("--reps", type=int, default=5, help="repeticiones por caso")
    p.add_argument("--out", default="bench/resultados", help="carpeta de salida")
    p.add_argument("--timeout", type=int, default=3600, help="segundos maximos por ejecucion")
    p.add_argument("--mpirun-args", default="", help='opciones extra para mpirun, ej. "--oversubscribe"')
    return p.parse_args()


def encrypt(args, key, folder):
    path = os.path.join(folder, f"llave_{key}.des")
    subprocess.run([args.encrypt, args.text, str(key), path],
                   check=True, stdout=subprocess.DEVNULL)
    return path


def run_once(args, n, cipher_path):
    cmd = ["mpirun", "-np", str(n), *args.mpirun_args.split(),
           args.bin, cipher_path, args.phrase, str(args.upper)]
    out = subprocess.run(cmd, capture_output=True, text=True,
                         timeout=args.timeout, check=True).stdout
    # RESULTADO,N,llave,proceso,segundos
    for line in out.splitlines():
        if line.startswith("RESULTADO,"):
            _, rn, key, rank, secs = line.split(",")
            return int(rn), int(key), int(rank), float(secs)
    raise RuntimeError("bruteforce no imprimio la linea RESULTADO:\n" + out)


def main():
    args = parse_args()
    bad = [k for k in args.keys if not 0 <= k < args.upper]
    if bad:
        sys.exit(f"Llaves fuera de [0, {args.upper}): {bad}")
    os.makedirs(args.out, exist_ok=True)

    raw = []
    with tempfile.TemporaryDirectory() as tmp:
        ciphers = {k: encrypt(args, k, tmp) for k in args.keys}
        total = args.reps * len(args.keys) * len(args.procs)
        done = 0
        # Las repeticiones van por fuera para repartir entre todos los casos
        # los cambios de carga o temperatura de la maquina.
        for rep in range(1, args.reps + 1):
            for key in args.keys:
                for n in args.procs:
                    done += 1
                    _, found, rank, secs = run_once(args, n, ciphers[key])
                    if found != key:
                        sys.exit(f"Error: llave={key} N={n} encontro {found}")
                    raw.append({"llave": key, "n": n, "rep": rep,
                                "proceso": rank, "segundos": secs})
                    print(f"[{done}/{total}] llave={key} N={n} rep={rep} "
                          f"proceso={rank} {secs:.3f}s", flush=True)

    with open(os.path.join(args.out, "crudo.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["llave", "n", "rep", "proceso", "segundos"])
        w.writeheader()
        w.writerows(raw)

    summary = []
    for key in args.keys:
        t1 = None
        for n in sorted(args.procs):
            times = [r["segundos"] for r in raw if r["llave"] == key and r["n"] == n]
            mean = statistics.mean(times)
            if n == 1:
                t1 = mean
            speedup = t1 / mean if t1 is not None else ""
            summary.append({
                "llave": key, "n": n, "reps": len(times),
                "promedio": f"{mean:.6f}",
                "desviacion": f"{statistics.stdev(times):.6f}" if len(times) > 1 else "0",
                "minimo": f"{min(times):.6f}", "maximo": f"{max(times):.6f}",
                "speedup": f"{speedup:.4f}" if speedup != "" else "",
                "eficiencia": f"{speedup / n:.4f}" if speedup != "" else "",
                "proceso": next(r["proceso"] for r in raw if r["llave"] == key and r["n"] == n),
            })

    with open(os.path.join(args.out, "resumen.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(summary[0].keys()))
        w.writeheader()
        w.writerows(summary)

    print(f"\nResultados en {args.out}/crudo.csv y {args.out}/resumen.csv")
    if 1 not in args.procs:
        print("Aviso: sin N=1 no se puede calcular el speedup.")


if __name__ == "__main__":
    main()
