/* Uso: ./decrypt_file texto_cifrado.des 123456 texto_restaurado.txt */
#include "des_crypto.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_key(const char *text, long *key) {
    if (!text || !*text || !key || *text == '-' || *text == '+') return -1;
    errno = 0;
    char *end = NULL;
    unsigned long long k = strtoull(text, &end, 10);
    if (errno || end == text || *end || k >= DES_EFFECTIVE_KEY_COUNT || k > LONG_MAX) return -1;
    *key = (long)k;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <archivo.des> <llave_decimal> <texto_restaurado.txt>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], argv[3]) == 0) {
        fprintf(stderr, "La salida debe tener un nombre diferente al archivo cifrado.\n");
        return EXIT_FAILURE;
    }
    long key;
    if (parse_key(argv[2], &key) != 0) {
        fprintf(stderr, "Llave decimal fuera de [0, 2^56).\n");
        return EXIT_FAILURE;
    }
    FILE *in = fopen(argv[1], "rb");
    if (!in) {
        perror("No se pudo abrir el archivo cifrado");
        return EXIT_FAILURE;
    }
    if (fseek(in, 0, SEEK_END) || ftell(in) < 0) {
        fprintf(stderr, "No se pudo leer el tamano del cifrado.\n");
        fclose(in);
        return EXIT_FAILURE;
    }
    long length = ftell(in);
    if (length <= 0 || length > INT_MAX || length % DES_BLOCK_BYTES != 0 ||
        fseek(in, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Cifrado invalido: longitud no multiplo de 8 o demasiado grande.\n");
        fclose(in);
        return EXIT_FAILURE;
    }
    unsigned char *buf = malloc((size_t)length);
    if (!buf) {
        fclose(in);
        fprintf(stderr, "Memoria insuficiente.\n");
        return EXIT_FAILURE;
    }
    if (fread(buf, 1, (size_t)length, in) != (size_t)length) {
        fprintf(stderr, "No se pudo leer el cifrado completo.\n");
        free(buf);
        fclose(in);
        return EXIT_FAILURE;
    }
    fclose(in);
    if (des_decrypt_checked(key, (char *)buf, (int)length) != 0) {
        fprintf(stderr, "El descifrado DES fallo.\n");
        free(buf);
        return EXIT_FAILURE;
    }
    size_t plain_len = 0;
    if (des_pkcs7_unpad(buf, (size_t)length, &plain_len) != 0) {
        fprintf(stderr, "Padding invalido: llave incorrecta o binario danado.\n");
        free(buf);
        return EXIT_FAILURE;
    }
    FILE *out = fopen(argv[3], "wb");
    if (!out) {
        perror("No se pudo abrir la salida");
        free(buf);
        return EXIT_FAILURE;
    }
    int bad = (fwrite(buf, 1, plain_len, out) != plain_len);
    if (fclose(out) != 0) bad = 1;
    free(buf);
    if (bad) {
        fprintf(stderr, "No se pudo escribir el archivo restaurado.\n");
        return EXIT_FAILURE;
    }
    printf("Descifrado: %zu bytes restaurados en %s\n", plain_len, argv[3]);
    return EXIT_SUCCESS;
}
