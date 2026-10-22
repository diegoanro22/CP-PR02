#!/bin/sh
# Levanta el servidor SSH (lo necesita mpirun en los workers) y luego corre el comando pedido.
service ssh start >/dev/null
exec "$@"
