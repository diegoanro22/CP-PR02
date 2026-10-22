# Guía del clúster MPI con Docker (Proyecto 2, Parte 2)

Pasos para que cada integrante levante su nodo y el maestro lance la búsqueda en todas las máquinas. Todos los comandos con `$` van en la terminal de la computadora; los que tienen `#` van **dentro del contenedor**.

## Resumen

1. Cada uno instala Docker, clona el repo y construye la imagen.
2. Todos se conectan al mismo router y abren el firewall para esa red.
3. Cada uno arranca su contenedor con `--network host`.
4. El maestro copia su llave SSH a los workers.
5. El maestro crea `cluster_hosts` y corre `mpirun`.

---

## 1. Instalar Docker

| Sistema | Qué instalar |
|---|---|
| Linux | `docker.io` (Ubuntu/Debian) o `docker` (Arch). Agregarse al grupo: `sudo usermod -aG docker $USER` y volver a iniciar sesión. |
| Windows | Docker Desktop **4.34 o más reciente**, con WSL 2. En *Settings > Resources > Network* activar **Enable host networking**. |
| macOS | Docker Desktop **4.34 o más reciente**. En *Settings > Resources > Network* activar **Enable host networking**. |

Comprobar: `$ docker run --rm hello-world`

## 2. Construir la imagen

**Todos deben construir desde el mismo commit.** Si las versiones de OpenMPI o del programa no coinciden entre máquinas, `mpirun` falla o se queda congelado.

```bash
$ git clone <url-del-repo>
$ cd CP-PR02
$ git pull                      # asegurarse de tener lo último
$ docker build -t mpi-cluster-image .
```

La imagen trae Ubuntu 22.04, OpenMPI, SSH configurado y el programa ya compilado en `/proyecto`.

Validación rápida (la tabla del enunciado):

```bash
$ docker run --rm mpi-cluster-image mpicc --version          # gcc ... 11.4.0
$ docker run --rm mpi-cluster-image mpirun -np 2 echo "Proceso MPI ejecutado"
```

## 3. Red y firewall

1. Conectarse al router del equipo (cable o Wi-Fi).
2. Anotar la IP que asignó el router (normalmente `192.168.1.X` o `192.168.0.X`):
   - Linux: `ip -4 addr`
   - Windows: `ipconfig` (adaptador Wi-Fi o Ethernet)
   - macOS: `ipconfig getifaddr en0`
3. Permitir el tráfico que viene de la red del router. OpenMPI abre puertos al azar, así que la regla es **por subred**, no por puerto. Cambiar `192.168.1.0/24` por la subred real:

| Sistema | Comando |
|---|---|
| Windows (PowerShell como administrador) | `Set-NetConnectionProfile -InterfaceAlias "Wi-Fi" -NetworkCategory Private` y luego `New-NetFirewallRule -DisplayName "Cluster MPI" -Direction Inbound -Action Allow -Profile Private -RemoteAddress 192.168.1.0/24` |
| Linux con ufw | `sudo ufw allow from 192.168.1.0/24` |
| Linux con firewalld | `sudo firewall-cmd --zone=trusted --add-source=192.168.1.0/24` |
| macOS | *Ajustes > Red > Firewall*: desactivarlo durante la práctica o permitir conexiones entrantes a Docker. |

Al terminar la práctica se puede borrar la regla (en Windows: `Remove-NetFirewallRule -DisplayName "Cluster MPI"`).

## 4. Arrancar el contenedor (todos)

```bash
$ docker run -it --network host --name mpi_node mpi-cluster-image
```

El SSH del contenedor arranca solo y escucha en el **puerto 2222** (no el 22, para no chocar con el SSH que pueda tener la computadora).

**Revisar que el contenedor ve la red del router:**

```bash
# ip -4 addr
```

Debe aparecer la IP del paso 3 (`192.168.1.X`). Si solo aparecen IPs como `192.168.65.X` o `172.17.X.X`, el contenedor no está en la red física; ver la sección 8.

Si se sale del contenedor, se vuelve a entrar con `$ docker start -ai mpi_node`. **No borrar el contenedor** (`docker rm`) hasta terminar: se perderían las llaves SSH.

## 5. Llaves SSH (Paso A)

En el contenedor del **maestro**:

```bash
# ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa
# cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
# cat ~/.ssh/id_rsa.pub
```

Copiar la línea que imprime el último comando (empieza con `ssh-rsa`) y pasarla a cada worker (por chat, por ejemplo). En el contenedor de **cada worker**:

```bash
# echo "ssh-rsa AAAA...la-linea-completa... root@..." >> ~/.ssh/authorized_keys
```

Probar desde el maestro, con la IP de cada worker:

```bash
# ssh 192.168.1.11 hostname
```

Debe responder el nombre del worker **sin pedir contraseña**.

## 6. Hostfile

En el contenedor del maestro, crear `/proyecto/cluster_hosts` (hay un ejemplo en `cluster_hosts.example`). El maestro va primero. `slots` es el número de procesos en cada máquina; no poner más que sus núcleos.

```bash
# cat > cluster_hosts <<'EOF'
192.168.1.10 slots=4 max_slots=4
192.168.1.11 slots=8 max_slots=8
192.168.1.12 slots=4 max_slots=4
EOF
```

Núcleos de cada máquina: `nproc` en Linux/WSL, `sysctl -n hw.ncpu` en macOS.

## 7. Ataque distribuido (Paso B)

Solo el maestro necesita el archivo cifrado: el proceso 0 lo lee y lo reparte a los demás. Desde la computadora del maestro:

```bash
$ docker cp cifrado_otro_equipo.des mpi_node:/proyecto/
```

En el contenedor del maestro (cambiar la subred, la frase, `upper` y el total de procesos, que es la suma de los `slots`):

```bash
# mpirun --hostfile cluster_hosts \
    --mca btl_tcp_if_include 192.168.1.0/24 --mca oob_tcp_if_include 192.168.1.0/24 \
    -np 16 ./bruteforce cifrado_otro_equipo.des "frase objetivo" 16777216 intercalado
```

La salida muestra los nodos que participaron y quién encontró la llave:

```
Nodos participantes (16 procesos):
  proceso 0 -> laptop-maestro (192.168.1.10)
  ...
Llave = 123456, encontrada por el proceso 5 en laptop-ana (192.168.1.11)
```

Esa salida es la captura que pide el enunciado. La última línea, `RESULTADO,N,llave,proceso,segundos`, trae el tiempo para comparar con la corrida local.

**Antes de usar el texto del otro equipo**, confirmar que lo cifraron igual que nosotros: DES en modo ECB, padding PKCS#7 y la misma conversión del número de llave a los 8 bytes de DES (`des_key_to_bytes` en `des/des_crypto.c`). Una forma de comprobarlo es pedirles un archivo cifrado con una llave pequeña conocida y ver que `bruteforce` la encuentra. Si su formato es distinto, el programa recorre la llave correcta sin reconocerla.

## 8. Problemas frecuentes

| Síntoma | Causa probable y solución |
|---|---|
| `mpirun` se queda congelado sin errores | Falta `--mca btl_tcp_if_include` / `oob_tcp_if_include`, o la subred no es la del router. |
| `ssh: connect to host ... port 2222: Connection refused` | El contenedor de ese worker no está corriendo, o su firewall bloquea. Revisar sección 3 y 4. |
| `Permission denied (publickey)` | La llave del maestro no está en el `authorized_keys` de ese worker (sección 5). |
| `There are not enough slots available` | `-np` es mayor que la suma de los `slots` del hostfile. |
| `ORTE was unable to reliably start one or more daemons` | SSH o firewall de algún worker. Probar `ssh <ip> hostname` desde el maestro a cada uno. |
| Errores de versión o símbolos de MPI entre nodos | Alguien construyó desde otro commit. Todos: `git pull` y `docker build` de nuevo. |
| `ip -4 addr` dentro del contenedor no muestra la IP del router (Windows/macOS) | Docker Desktop no está pasando la red física. Revisar que esté activado *Enable host networking* y la versión 4.34+. En Windows, la alternativa es instalar Docker dentro de WSL con red espejada (abajo). |

**Alternativa en Windows: Docker dentro de WSL con red espejada.** Requiere Windows 11 22H2 o más reciente.

1. Crear `C:\Users\<usuario>\.wslconfig` con:
   ```ini
   [wsl2]
   networkingMode=mirrored
   ```
2. En PowerShell: `wsl --shutdown` y volver a abrir Ubuntu. Ahora `ip -4 addr` en WSL debe mostrar la IP del router.
3. Dentro de Ubuntu (WSL): `sudo apt install docker.io`, `sudo usermod -aG docker $USER`, cerrar y abrir la terminal, y seguir esta guía desde la sección 2.
4. Permitir el tráfico entrante hacia WSL (PowerShell como administrador):
   `Set-NetFirewallHyperVVMSetting -Name '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' -DefaultInboundAction Allow`
