/*
 * Programa auxiliar: cifra un archivo de texto de 335 a 365
 * palabras mediante DES/ECB con padding PKCS#7 y genera un archivo BINARIO.
 *
 * Uso: ./encrypt_file texto_350.txt 123456 texto_cifrado.des
 */
#include "des_crypto.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_WORDS 335U
#define MAX_WORDS 365U

static int parse_key(const char *input, long *key) {
    char *end = NULL;
    errno = 0;
    if (input == NULL || key == NULL || input[0] == '\0' ||
        input[0] == '-' || input[0] == '+') {
        return -1;
    }
    unsigned long long parsed = strtoull(input, &end, 10);
    if (errno != 0 || end == input || *end != '\0' ||
        parsed >= DES_EFFECTIVE_KEY_COUNT || parsed > LONG_MAX) {
        return -1;
    }
    *key = (long)parsed;
    return 0;
}

static int read_file(const char *path, unsigned char **out, size_t *size) {
    FILE *in = fopen(path, "rb");
    if (in == NULL) {
        perror("No se pudo abrir el archivo de texto");
        return -1;
    }
    if (fseek(in, 0, SEEK_END) != 0) {
        perror("No se pudo calcular el tamano");
        fclose(in);
        return -1;
    }
    long len = ftell(in);
    if (len < 0 || len > INT_MAX - (int)DES_BLOCK_BYTES) {
        fprintf(stderr, "Tamano de archivo no admitido.\n");
        fclose(in);
        return -1;
    }
    if (fseek(in, 0, SEEK_SET) != 0) {
        perror("No se pudo regresar al inicio");
        fclose(in);
        return -1;
    }
    unsigned char *buffer = malloc((size_t)len + 1U);
    if (buffer == NULL) {
        fprintf(stderr, "No hay memoria suficiente.\n");
        fclose(in);
        return -1;
    }
    if (fread(buffer, 1, (size_t)len, in) != (size_t)len) {
        fprintf(stderr, "No se pudo leer el archivo completo.\n");
        free(buffer);
        fclose(in);
        return -1;
    }
    fclose(in);
    buffer[len] = '\0';
    *out = buffer;
    *size = (size_t)len;
    return 0;
}

/* Se considera palabra cada token separado por espacios, tabs o saltos. */
static size_t word_count(const unsigned char *data, size_t len) {
    size_t words = 0;
    int inside_word = 0;
    for (size_t i = 0; i < len; ++i) {
        if (isspace(data[i])) {
            inside_word = 0;
        } else if (!inside_word) {
            ++words;
            inside_word = 1;
        }
    }
    return words;
}

static int write_file(const char *path, const unsigned char *data, size_t len) {
    FILE *out = fopen(path, "wb");
    if (out == NULL) {
        perror("No se pudo crear el archivo cifrado");
        return -1;
    }
    if (fwrite(data, 1, len, out) != len) {
        fprintf(stderr, "No se pudo escribir el cifrado completo.\n");
        fclose(out);
        return -1;
    }
    if (fclose(out) != 0) {
        perror("Error al cerrar el archivo cifrado");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <texto_entrada.txt> <llave_decimal> <salida.des>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], argv[3]) == 0) {
        fprintf(stderr, "La salida debe tener un nombre diferente al archivo original.\n");
        return EXIT_FAILURE;
    }
    long key = 0;
    if (parse_key(argv[2], &key) != 0) {
        fprintf(stderr, "Llave invalida: ingrese un entero decimal entre 0 y 2^56-1.\n");
        return EXIT_FAILURE;
    }
    unsigned char *input = NULL;
    size_t original_size = 0;
    if (read_file(argv[1], &input, &original_size) != 0) {
        return EXIT_FAILURE;
    }
    if (memchr(input, '\0', original_size) != NULL) {
        fprintf(stderr, "El archivo de entrada contiene bytes nulos; debe ser texto plano.\n");
        free(input);
        return EXIT_FAILURE;
    }
    size_t words = word_count(input, original_size);
    if (words < MIN_WORDS || words > MAX_WORDS) {
        fprintf(stderr, "El texto contiene %zu palabras; se requieren entre %u y %u.\n",
                words, MIN_WORDS, MAX_WORDS);
        free(input);
        return EXIT_FAILURE;
    }
    unsigned char *cipher = NULL;
    size_t encrypted_size = 0;
    int status = des_pkcs7_pad(input, original_size, &cipher, &encrypted_size);
    free(input);
    if (status != 0) {
        fprintf(stderr, "Error al agregar padding PKCS#7 (%d).\n", status);
        return EXIT_FAILURE;
    }
    status = des_encrypt_checked(key, (char *)cipher, (int)encrypted_size);
    if (status != 0) {
        fprintf(stderr, "Error DES al cifrar (%d).\n", status);
        free(cipher);
        return EXIT_FAILURE;
    }
    status = write_file(argv[3], cipher, encrypted_size);
    free(cipher);
    if (status != 0) {
        return EXIT_FAILURE;
    }
    printf("Cifrado completado.\n");
    printf("Palabras: %zu (rango requerido: %u-%u).\n", words, MIN_WORDS, MAX_WORDS);
    printf("Llave DES efectiva (decimal): %ld.\n", key);
    printf("Longitud original: %zu bytes.\n", original_size);
    printf("Longitud cifrada con PKCS#7: %zu bytes.\n", encrypted_size);
    printf("Archivo binario: %s\n", argv[3]);
    return EXIT_SUCCESS;
}
