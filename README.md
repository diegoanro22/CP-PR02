# CP-PR02: Brute-force de DES con OpenMPI

Proyecto 2 de Computación Paralela. Búsqueda por fuerza bruta de la llave privada DES de un texto cifrado, distribuyendo el espacio de llaves entre procesos con OpenMPI.

## Compilar y ejecutar

```bash
make                       # compila bruteforce y las utilidades de des/
des/encrypt_file des/texto_350_palabras.txt 70000 cifrado.des
mpirun -np 3 ./bruteforce cifrado.des " los mismos del proyecto 1 " 120000
```

Argumentos de `bruteforce`: `<cifrado.des> <frase> <upper> [bloques|intercalado]`

- Busca llaves en `[0, upper)`, con `upper` hasta 2^56.
- `bloques` (por defecto): cada proceso recorre un bloque contiguo del rango.
- `intercalado`: el proceso `id` prueba `id, id+N, id+2N, ...`.
- `make TEST_EVERY=K` revisa el aviso de terminación cada K llaves.

La última línea de salida, `RESULTADO,N,llave,proceso,segundos`, es la que usa `bench/bench.py`.
