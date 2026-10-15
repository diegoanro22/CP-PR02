# CP-PR02: Brute-force de DES con OpenMPI

Proyecto 2 de Computación Paralela. Búsqueda por fuerza bruta de la llave privada DES de un texto cifrado, distribuyendo el espacio de llaves entre procesos con OpenMPI.

## Compilar y ejecutar

```bash
mpicc bruteforce_.c -o bruteforce -I/usr/include/tirpc -ltirpc
mpirun -np 3 ./bruteforce
```
