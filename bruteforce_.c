#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <rpc/des_crypt.h>

// cada cuantas llaves se revisa el aviso de terminacion (compilar con -DTEST_EVERY=K)
#ifndef TEST_EVERY
#define TEST_EVERY 1
#endif




void decrypt(long key, char *ciph, int len){
  long k = 0;
  for(int i=0; i<8; ++i){
    key <<= 1;
    k += (key & (0xFE << i*8));
  }
  des_setparity((char *)&k);
  ecb_crypt((char *)&k, (char *) ciph, 16, DES_DECRYPT);
}




void encrypt(long key, char *ciph){
  long k = 0;
  for(int i=0; i<8; ++i){
    key <<= 1;
    k += (key & (0xFE << i*8));
  }
  des_setparity((char *)&k); 
  ecb_crypt((char *)&k, (char *) ciph, 16, DES_ENCRYPT);
}




char search[] = " los mismos del proyecto 1 ";
int tryKey(long key, char *ciph, int len){
  char temp[len+1]; 
  memcpy(temp, ciph, len);
  temp[len]=0;
  decrypt(key, temp, len);
  return strstr((char *)temp, search) != NULL;
}

char eltexto[] = " En equipos de 3 integrantes (los mismos del proyecto 1), deben utilizar MPI para mejorar un programa que encuentra la llave privada con la que fué cifrado un texto plano.  La búsqueda se hará probando todas las posibles combinaciones de llaves, hasta encontrar una que descifra el texto. ";
long the_key = 18014398509481984;


int main(int argc, char *argv[]){ 
  int N, id;
  long upper = (1L <<56); // limite superior de llaves DES: 2^56
  long mylower, myupper;
  MPI_Status st;
  MPI_Request req;

  int ciphlen = strlen(eltexto);
  MPI_Comm comm = MPI_COMM_WORLD;

  char cipher[ciphlen+1];
  memcpy(cipher, eltexto, ciphlen);
  cipher[ciphlen]=0;
  encrypt(the_key, cipher);

  MPI_Init(NULL, NULL);
  MPI_Comm_size(comm, &N);
  MPI_Comm_rank(comm, &id);

  long found = -1;   // llave que encontro este proceso (-1 = ninguna)
  long received = 0; // buffer exclusivo de MPI_Irecv: no se toca mientras esta pendiente
  int ready = 0;

  long range_per_node = upper / N;
  mylower = range_per_node * id;
  myupper = range_per_node * (id+1) -1;
  if(id == N-1){
    myupper = upper - 1; // el ultimo bloque llega hasta la ultima llave valida
  }
  printf("Proceso %d: rango [%ld, %ld]\n", id, mylower, myupper);

  MPI_Irecv(&received, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

  // todos empiezan a medir al mismo tiempo
  MPI_Barrier(comm);
  double start = MPI_Wtime();

  // el aviso de otro proceso se revisa cada TEST_EVERY llaves (1 = en cada llave)
  long untilTest = 0;
  for(long i = mylower; i<=myupper; ++i){
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
      printf("Llave = %ld\n\n", best.key);
      printf("%s\n", cipher);
    }else{
      printf("No se encontro la llave en [0, %ld)\n", upper);
      best.rank = -1;
    }
    // linea para bench.py: N, llave, proceso que la encontro, segundos
    printf("RESULTADO,%d,%ld,%d,%.6f\n", N, best.key, best.rank, elapsed);
  }

  MPI_Finalize();
  return 0;
}
