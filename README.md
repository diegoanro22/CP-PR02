# CP-PR02: búsqueda exhaustiva de llaves DES con OpenMPI

Proyecto 2 de Computación Paralela (Universidad del Valle de Guatemala). Dado un texto cifrado con DES, el programa prueba llaves hasta que una descifra el texto, que se reconoce porque contiene una frase clave. El espacio de llaves se reparte entre procesos con OpenMPI, y las mediciones analizan cómo cambia el speedup según la llave y el tipo de reparto.

## Contenido

| Ruta | Qué contiene |
|---|---|
| `bruteforce_.c` | Programa MPI: reparto de llaves, `tryKey`, aviso de terminación y cronómetro |
| `Makefile` | Compila `bruteforce` y las utilidades de `des/` |
| `des/` | Módulo de cifrado DES (`des_crypto.c`), utilidades `encrypt_file` y `decrypt_file`, pruebas y el texto de 350 palabras |
| `bench/` | `bench.py` (mediciones), `plot.py` (gráficas) y `resultados/` (datos y gráficas de las mediciones) |
| `Dockerfile`, `docker-entrypoint.sh` | Imagen del clúster de la Parte 2 (sección 6) |
| `CLUSTER.md` | Guía para que cada integrante levante su nodo del clúster |
| `cluster_hosts.example` | Hostfile de ejemplo para el clúster |
| `docs/diagramas/` | Diagramas de `tryKey`, `memcpy`, `strstr`, reparto de llaves y flujo MPI |

## 1. Preparar el entorno

El proyecto usa C con OpenMPI y OpenSSL. Funciona en Linux; en Windows se usa WSL y en macOS hay que indicar dónde está OpenSSL.

### Linux (Ubuntu o Debian)

```bash
sudo apt update
sudo apt install build-essential make openmpi-bin libopenmpi-dev libssl-dev python3 python3-matplotlib
```

`libtirpc-dev` no es necesario: el `Makefile` usa `ecb_crypt` de libtirpc si está disponible y, si no, la interfaz DES de OpenSSL. Ambos dan el mismo resultado.

### Windows (con WSL 2, recomendado)

1. En PowerShell como administrador: `wsl --install -d Ubuntu`, y reiniciar.
2. Abrir la terminal de Ubuntu e instalar los paquetes de la sección de Linux.
3. Clonar el repositorio **dentro del sistema de archivos de Linux** (por ejemplo en `~`), no en `/mnt/c/...`. Es más rápido y evita que Git convierta los finales de línea a CRLF (con CRLF el texto de prueba mide 2 500 bytes en lugar de 2 489).

Windows nativo (MinGW o MS-MPI) no está soportado.

### macOS (no probado)

```bash
brew install open-mpi openssl@3 python matplotlib
make OPENSSL_PREFIX=$(brew --prefix openssl@3)
```

Este camino **no se ha probado en macOS**. Se espera que funcione porque el código solo usa OpenSSL, OpenMPI y la biblioteca estándar de C, pero si falla se recomienda usar Linux o WSL. En los comandos de las secciones siguientes, agregar `OPENSSL_PREFIX=...` a cada `make`.

### Comprobar la instalación

```bash
mpicc --showme:version
```

Debe responder `Open MPI ...`. (En algunas instalaciones de Ubuntu `mpirun --version` falla por archivos de ayuda ausentes aunque OpenMPI funcione bien; no es un error del proyecto.)

## 2. Compilar

```bash
make
```

Compila `bruteforce` y, en `des/`, `encrypt_file`, `decrypt_file` y `test_des_crypto`. Opciones:

- `make BACKEND=openssl` o `make BACKEND=tirpc` fuerza el backend DES (por defecto se elige el disponible).
- `make TEST_EVERY=K` revisa el aviso de terminación cada K llaves (por defecto 1).
- `make clean` borra los ejecutables.

## 3. Cifrar un texto y buscar la llave

```bash
des/encrypt_file des/texto_350_palabras.txt 70000 cifrado.des
mpirun -np 3 ./bruteforce cifrado.des " los mismos del proyecto 1 " 120000
mpirun -np 3 ./bruteforce cifrado.des " los mismos del proyecto 1 " 120000 intercalado
```

`encrypt_file` recibe el texto (de 335 a 365 palabras), la llave y el archivo de salida. Genera un archivo binario con padding PKCS#7; no guarda la llave ni la frase.

`bruteforce` recibe: `<cifrado.des> <frase> <upper> [bloques|intercalado]`

- Busca llaves en `[0, upper)`, con `1 <= upper <= 2^56`. La llave usada para cifrar debe ser menor que `upper`.
- `bloques` (por defecto): cada proceso recorre un bloque contiguo del rango.
- `intercalado`: el proceso `id` prueba `id, id+N, id+2N, ...`.
- La última línea de la salida, `RESULTADO,N,llave,proceso,segundos`, es la que lee `bench/bench.py`. Si la llave no está en el rango, la llave del resultado es `-1`.

Para comprobar el cifrado, `des/decrypt_file cifrado.des 70000 texto_restaurado.txt` reconstruye el texto original (se puede comparar con `cmp des/texto_350_palabras.txt texto_restaurado.txt`).

Recorrer las 2^56 llaves es inviable (años de cómputo): por eso se limita la búsqueda con `upper`. En el equipo de las mediciones, un proceso prueba unas 57 000 llaves por segundo.

## 4. Pruebas

```bash
make -C des test
```

Comprueba la conversión de la llave, un vector de prueba oficial de DES, el padding PKCS#7, el cifrado y descifrado de textos largos y el manejo de errores.

## 5. Reproducir las mediciones

Desde la raíz del repositorio, con todo compilado (`make`):

```bash
python3 bench/bench.py --upper 4194304 --keys 123456 2097151 2097153 4194303 \
        --procs 1 2 3 4 --modos bloques intercalado --reps 5 --out bench/resultados/principal
python3 bench/plot.py bench/resultados/principal
```

La primera tanda dura cerca de una hora. Con `--reanudar` se continúa una tanda interrumpida sin repetir lo ya medido. Una prueba rápida (menos de un minuto):

```bash
python3 bench/bench.py --upper 65536 --keys 30000 32769 --procs 1 2 3 \
        --modos bloques intercalado --reps 2 --out /tmp/prueba
```

`bench.py` no necesita matplotlib; `plot.py` sí. Los datos y gráficas de las mediciones del informe están en `bench/resultados/`.

## 6. Clúster con Docker (Parte 2)

La guía paso a paso para todo el equipo (firewall, llaves SSH, hostfile y problemas frecuentes) está en [CLUSTER.md](CLUSTER.md).

Todos los integrantes construyen **la misma imagen**: si las versiones de OpenMPI no coinciden entre máquinas, `mpirun` falla o se cuelga.

```bash
docker build -t mpi-cluster-image .
docker run -it --network host --name mpi_node mpi-cluster-image
```

La imagen ya trae el programa compilado en `/proyecto`, levanta SSH al arrancar y permite correr MPI como root. El SSH del contenedor escucha en el **puerto 2222** (no 22, que con `--network host` puede estar ocupado por el SSH del sistema), y el cliente ya está configurado para usarlo.

**Paso A, llaves SSH.** En el contenedor del maestro:

```bash
ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa
cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
cat ~/.ssh/id_rsa.pub     # copiar esta línea al ~/.ssh/authorized_keys de cada worker
ssh 192.168.1.11 hostname # desde el maestro: debe responder sin pedir contraseña
```

**Paso B, ataque distribuido.** Solo el maestro necesita el archivo cifrado (el proceso 0 lo lee y lo reparte). Copiarlo con `docker cp cifrado.des mpi_node:/proyecto/`, crear `cluster_hosts` a partir de `cluster_hosts.example` y correr, cambiando la subred por la del router:

```bash
mpirun --hostfile cluster_hosts \
       --mca btl_tcp_if_include 192.168.1.0/24 --mca oob_tcp_if_include 192.168.1.0/24 \
       -np <TOTAL_SLOTS> ./bruteforce cifrado.des "<frase>" <upper> intercalado
```

La salida empieza con la lista de nodos participantes (`proceso N -> host (IP)`) y termina indicando qué proceso y qué máquina encontró la llave.

En Windows y macOS hace falta Docker Desktop 4.34 o más reciente con *Enable Host Networking* activado, y una regla de firewall que permita conexiones entrantes en la red privada (el puerto 2222 y los puertos dinámicos de OpenMPI).

## 7. Problemas frecuentes

| Mensaje | Causa y solución |
|---|---|
| `mpicc: command not found` | Falta OpenMPI. Instalar `openmpi-bin` y `libopenmpi-dev` (o `open-mpi` con Homebrew). |
| `openssl/des.h: No such file or directory` o `cannot find -lcrypto` | Falta OpenSSL. En Linux, `libssl-dev`; en macOS, usar `OPENSSL_PREFIX` como en la sección 1. |
| `rpc/des_crypt.h: No such file or directory` | Es el error del `bruteforce_.c` original, que dependía de libtirpc. El programa actual usa `des/des_crypto.c` y no tiene ese problema. |
| `There are not enough slots available` | Se pidieron más procesos que núcleos. Agregar `--oversubscribe`: `mpirun --oversubscribe -np 8 ./bruteforce ...` |
| `mpirun has detected an attempt to run as root` | Ejecutar con un usuario normal o agregar `--allow-run-as-root` (por ejemplo, dentro de Docker). |
