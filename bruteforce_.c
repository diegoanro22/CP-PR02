#define _DEFAULT_SOURCE // getifaddrs con -std=c11
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mpi.h>
#include "des_crypto.h"

// texto "host (ip)" que identifica la maquina de cada proceso
#define NODE_LEN (MPI_MAX_PROCESSOR_NAME + INET_ADDRSTRLEN + 4)

// cada cuantas llaves se revisa el aviso de los demas procesos
#ifndef TEST_EVERY
#define TEST_EVERY 1
#endif

// frase clave y buffer de trabajo de tryKey
char *search = NULL;
char *temp = NULL;

int tryKey(long key, char *ciph, int len){
  memcpy(temp, ciph, len);
  temp[len] = 0;
  decrypt(key, temp, len);
  if(strstr(temp, search) == NULL)
    return 0;
  // descarta falsos positivos: el padding debe ser valido y no debe haber bytes nulos
  size_t plain_len;
  return des_pkcs7_unpad((unsigned char *)temp, len, &plain_len) == 0
      && strlen(temp) == (size_t)len;
}

// lee un entero en [min, max]; devuelve -1 si no es valido
static int parse_long(const char *s, long min, long max, long *out){
  char *end = NULL;
  errno = 0;
  long v = strtol(s, &end, 10);
  if(errno != 0 || end == s || *end != '\0' || v < min || v > max)
    return -1;
  *out = v;
  return 0;
}

// devuelve la longitud del cifrado o -1
static int read_cipher(const char *path, char **out){
  FILE *f = fopen(path, "rb");
  if(f == NULL){
    perror("No se pudo abrir el archivo cifrado");
    return -1;
  }
  if(fseek(f, 0, SEEK_END) != 0){
    fclose(f);
    return -1;
  }
  long len = ftell(f);
  if(len <= 0 || len >= INT_MAX || len % DES_BLOCK_BYTES != 0 || fseek(f, 0, SEEK_SET) != 0){
    fprintf(stderr, "Cifrado invalido: la longitud debe ser multiplo de 8.\n");
    fclose(f);
    return -1;
  }
  char *buf = malloc(len);
  if(buf == NULL || fread(buf, 1, len, f) != (size_t)len){
    fprintf(stderr, "No se pudo leer el cifrado completo.\n");
    free(buf);
    fclose(f);
    return -1;
  }
  fclose(f);
  *out = buf;
  return (int)len;
}


// IPv4 de la red fisica: salta loopback y las interfaces virtuales de Docker
static void local_ip(char *out, size_t size){
  snprintf(out, size, "sin IP");
  struct ifaddrs *list;
  if(getifaddrs(&list) != 0)
    return;
  for(struct ifaddrs *it = list; it != NULL; it = it->ifa_next){
    if(it->ifa_addr == NULL || it->ifa_addr->sa_family != AF_INET)
      continue;
    if((it->ifa_flags & IFF_LOOPBACK) || strncmp(it->ifa_name, "docker", 6) == 0
       || strncmp(it->ifa_name, "br-", 3) == 0 || strncmp(it->ifa_name, "veth", 4) == 0)
      continue;
    inet_ntop(AF_INET, &((struct sockaddr_in *)it->ifa_addr)->sin_addr, out, size);
    break;
  }
  freeifaddrs(list);
}

// arma "host (ip)" para este proceso
static void node_name(char out[NODE_LEN]){
  char host[MPI_MAX_PROCESSOR_NAME];
  char ip[INET_ADDRSTRLEN];
  int host_len;
  MPI_Get_processor_name(host, &host_len);
  local_ip(ip, sizeof ip);
  snprintf(out, NODE_LEN, "%s (%s)", host, ip);
}

int main(int argc, char *argv[]){
  int N, id;
  long upper;
  long mylower, myupper, step;
  MPI_Request req;
  MPI_Comm comm = MPI_COMM_WORLD;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(comm, &N);
  MPI_Comm_rank(comm, &id);

  // uso: bruteforce <cifrado.des> <frase> <upper> [bloques|intercalado]
  int interleaved = argc == 5 && strcmp(argv[4], "intercalado") == 0;
  int args_ok = (argc == 4 || (argc == 5 && (interleaved || strcmp(argv[4], "bloques") == 0)))
             && parse_long(argv[3], 1, (long)DES_EFFECTIVE_KEY_COUNT, &upper) == 0
             && argv[2][0] != '\0';
  if(!args_ok){
    if(id == 0)
      fprintf(stderr, "Uso: %s <cifrado.des> <frase> <upper> [bloques|intercalado]\n"
                      "  busca llaves en [0, upper), con 1 <= upper <= 2^56\n", argv[0]);
    MPI_Finalize();
    return EXIT_FAILURE;
  }
  search = argv[2];

  // el proceso 0 lee el archivo y lo reparte a los demas
  char *cipher = NULL;
  int ciphlen = 0;
  if(id == 0)
    ciphlen = read_cipher(argv[1], &cipher);
  MPI_Bcast(&ciphlen, 1, MPI_INT, 0, comm);
  if(ciphlen < 0){
    MPI_Finalize();
    return EXIT_FAILURE;
  }
  if(id != 0)
    cipher = malloc(ciphlen);
  temp = malloc(ciphlen + 1);
  if(cipher == NULL || temp == NULL){
    fprintf(stderr, "Proceso %d: sin memoria.\n", id);
    MPI_Abort(comm, EXIT_FAILURE);
  }
  MPI_Bcast(cipher, ciphlen, MPI_CHAR, 0, comm);

  long found = -1;   // llave que encontro este proceso (-1 = ninguna)
  long received = 0;
  int ready = 0;

  if(interleaved){
    // intercalado: el proceso id prueba id, id+N, id+2N, ...
    mylower = id;
    myupper = upper - 1;
    step = N;
  }else{
    // bloques contiguos: el proceso id prueba [id*R, (id+1)*R - 1]
    long range_per_node = upper / N;
    mylower = range_per_node * id;
    myupper = range_per_node * (id+1) -1;
    if(id == N-1){
      myupper = upper - 1;
    }
    step = 1;
  }
  // el proceso 0 junta en que maquina corre cada proceso, para mostrar los nodos del cluster
  char node[NODE_LEN];
  node_name(node);
  char *nodes = NULL;
  if(id == 0){
    nodes = malloc((size_t)N * NODE_LEN);
    if(nodes == NULL){
      fprintf(stderr, "Proceso 0: sin memoria.\n");
      MPI_Abort(comm, EXIT_FAILURE);
    }
  }
  MPI_Gather(node, NODE_LEN, MPI_CHAR, nodes, NODE_LEN, MPI_CHAR, 0, comm);
  if(id == 0){
    printf("Nodos participantes (%d procesos):\n", N);
    for(int p = 0; p < N; p++)
      printf("  proceso %d -> %s\n", p, nodes + (size_t)p * NODE_LEN);
    fflush(stdout);
  }

  printf("Proceso %d en %s: rango [%ld, %ld] paso %ld\n", id, node, mylower, myupper, step);

  MPI_Irecv(&received, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

  MPI_Barrier(comm);
  double start = MPI_Wtime();

  long untilTest = 0;
  for(long i = mylower; i<=myupper; i += step){
    if(untilTest-- == 0){
      untilTest = TEST_EVERY - 1;
      MPI_Test(&req, &ready, MPI_STATUS_IGNORE);
      if(ready)
        break;
    }

    if(tryKey(i, cipher, ciphlen)){
      found = i;
      printf("Proceso %d en %s encontro la llave\n", id, node);
      for(int node=0; node<N; node++){
        MPI_Send(&found, 1, MPI_LONG, node, 0, comm);
      }
      break;
    }
  }

  // si nadie encontro la llave no llega ningun aviso
  struct { long key; int rank; } mine = {found, id}, best;
  MPI_Allreduce(&mine, &best, 1, MPI_LONG_INT, MPI_MAXLOC, comm);
  double elapsed = MPI_Wtime() - start;

  if(best.key >= 0){
    MPI_Wait(&req, MPI_STATUS_IGNORE);
  }else{
    MPI_Cancel(&req);
    MPI_Wait(&req, MPI_STATUS_IGNORE);
  }

  if(id==0){
    if(best.key >= 0){
      decrypt(best.key, cipher, ciphlen);
      size_t plain_len = ciphlen;
      des_pkcs7_unpad((unsigned char *)cipher, ciphlen, &plain_len);
      printf("Llave = %ld, encontrada por el proceso %d en %s\n\n",
             best.key, best.rank, nodes + (size_t)best.rank * NODE_LEN);
      fwrite(cipher, 1, plain_len, stdout);
      printf("\n");
    }else{
      printf("No se encontro la llave en [0, %ld)\n", upper);
      best.rank = -1;
    }
    // N, llave, proceso, segundos (la lee bench.py)
    printf("RESULTADO,%d,%ld,%d,%.6f\n", N, best.key, best.rank, elapsed);
  }

  free(nodes);
  free(cipher);
  free(temp);
  MPI_Finalize();
  return 0;
}
