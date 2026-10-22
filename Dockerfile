# Imagen del cluster MPI (Parte 2). Todos los nodos deben usar esta misma imagen:
# si las versiones de OpenMPI no coinciden entre maquinas, mpirun falla o se cuelga.
#   docker build -t mpi-cluster-image .
#   docker run -it --network host --name mpi_node mpi-cluster-image
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# libssl-dev: el modulo DES usa OpenSSL (no viene en la lista base del enunciado)
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential openmpi-bin libopenmpi-dev openssh-server openssh-client \
        libssl-dev iproute2 iputils-ping \
    && rm -rf /var/lib/apt/lists/*

# SSH sin intervencion para que mpirun lance procesos en los workers.
# Puerto 2222: con --network host el contenedor comparte los puertos de la
# maquina, y el 22 puede estar ocupado por el SSH del propio sistema.
RUN mkdir -p /var/run/sshd /root/.ssh && chmod 700 /root/.ssh \
    && sed -i 's/^#\?PermitRootLogin .*/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && sed -i 's/^#\?Port .*/Port 2222/' /etc/ssh/sshd_config \
    && printf '%s\n' 'Host *' '    StrictHostKeyChecking no' \
        '    UserKnownHostsFile /dev/null' '    Port 2222' >> /etc/ssh/ssh_config

# OpenMPI no deja correr como root si no se confirma explicitamente
ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

# mismo codigo y misma ruta en todos los nodos: mpirun busca ./bruteforce en /proyecto
WORKDIR /proyecto
COPY . .
RUN make clean && make BACKEND=openssl

COPY docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
# Git en Windows convierte los .sh a CRLF y el '#!/bin/sh' deja de funcionar
# ("exec ...docker-entrypoint.sh: no such file or directory"): aqui se quita el \r.
RUN sed -i 's/\r$//' /usr/local/bin/docker-entrypoint.sh \
    && chmod +x /usr/local/bin/docker-entrypoint.sh
ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["/bin/bash"]
