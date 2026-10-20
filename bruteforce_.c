#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <mpi.h>
#include "des_crypto.h"

// cada cuantas llaves se revisa el aviso de terminacion (compilar con -DTEST_EVERY=K)
#ifndef TEST_EVERY
#define TEST_EVERY 1
#endif

// encrypt y decrypt ahora vienen de des/des_crypto.c: cifran el buffer completo
// (no solo 16 bytes) y arman la llave de 56 bits sin overflow.




// frase clave y buffer de trabajo de tryKey (se reserva una sola vez en main)
char *search = NULL;
char *temp = NULL;

int tryKey(long key, char *ciph, int len){
  memcpy(temp, ciph, len);
  temp[len] = 0; // terminador despues de los datos para que strstr no se salga
  decrypt(key, temp, len);
  if(strstr(temp, search) == NULL)
    return 0;
  // con una frase corta una llave incorrecta puede contenerla por casualidad.
  // Se descartan esos falsos positivos (solo se revisa si hubo match):
  //  - el padding PKCS#7 debe ser valido
  //  - el texto plano no tiene bytes nulos (encrypt_file lo exige), asi que
  //    strlen debe llegar hasta el final del buffer
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

// lee el binario completo creado por des/encrypt_file; devuelve su longitud o -1
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

  // solo el proceso 0 lee el archivo y lo reparte: asi funciona aunque las
  // maquinas del cluster no compartan sistema de archivos
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
  long received = 0; // buffer exclusivo de MPI_Irecv: no se toca mientras esta pendiente
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
      myupper = upper - 1; // el ultimo bloque llega hasta la ultima llave valida
    }
    step = 1;
  }
  printf("Proceso %d: rango [%ld, %ld] paso %ld\n", id, mylower, myupper, step);

  MPI_Irecv(&received, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

  // todos empiezan a medir al mismo tiempo
  MPI_Barrier(comm);
  double start = MPI_Wtime();

  // el aviso de otro proceso se revisa cada TEST_EVERY llaves (1 = en cada llave)
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
      printf("Proceso %d encontro la llave\n", id);
      for(int node=0; node<N; node++){
        MPI_Send(&found, 1, MPI_LONG, node, 0, comm);
      }
      break;
    }
  }

  // Si la llave no esta en el rango nadie envia el aviso. Con esta reduccion
  // todos saben si hubo llave y quien la encontro, sin quedarse esperando.
  struct { long key; int rank; } mine = {found, id}, best;
  MPI_Allreduce(&mine, &best, 1, MPI_LONG_INT, MPI_MAXLOC, comm);
  double elapsed = MPI_Wtime() - start;

  if(best.key >= 0){
    MPI_Wait(&req, MPI_STATUS_IGNORE); // el aviso llega a todos, incluido quien lo envio
  }else{
    MPI_Cancel(&req);
    MPI_Wait(&req, MPI_STATUS_IGNORE);
  }

  if(id==0){
    if(best.key >= 0){
      decrypt(best.key, cipher, ciphlen);
      size_t plain_len = ciphlen;
      des_pkcs7_unpad((unsigned char *)cipher, ciphlen, &plain_len); // quita el padding si es valido
      printf("Llave = %ld\n\n", best.key);
      fwrite(cipher, 1, plain_len, stdout);
      printf("\n");
    }else{
      printf("No se encontro la llave en [0, %ld)\n", upper);
      best.rank = -1;
    }
    // linea para bench.py: N, llave, proceso que la encontro, segundos
    printf("RESULTADO,%d,%ld,%d,%.6f\n", N, best.key, best.rank, elapsed);
  }

  free(cipher);
  free(temp);
  MPI_Finalize();
  return 0;
}
