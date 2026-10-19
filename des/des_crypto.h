#ifndef DES_CRYPTO_H
#define DES_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

/* DES utiliza una llave efectiva de 56 bits y bloques de 8 bytes. */
#define DES_BLOCK_BYTES 8U
#define DES_EFFECTIVE_KEY_BITS 56U
#define DES_EFFECTIVE_KEY_COUNT (UINT64_C(1) << DES_EFFECTIVE_KEY_BITS)

/*
 * Convierte una llave de 56 bits representada como long decimal en los ocho
 * bytes que consume DES. Cada byte tiene siete bits de llave y un bit de
 * paridad impar. Es una correspondencia biyectiva en [0, 2^56).
 * Retorno: 0 = exito, -1 = llave o puntero invalido.
 */
int des_key_to_bytes(long key, unsigned char out[DES_BLOCK_BYTES]);

/*
 * Ambas funciones trabajan IN PLACE sobre EXACTAMENTE len bytes.
 * PRECONDICION: len > 0 y multiplo de 8; no agregan ni retiran padding.
 * Se conserva la firma acordada por el equipo. Ante un uso invalido o un
 * fallo de la biblioteca criptografica, terminan con un error explicito.
 */
void encrypt(long key, char *buf, int len);
void decrypt(long key, char *buf, int len);

/* Alternativas que devuelven codigo de error, sin abortar el proceso. */
int des_encrypt_checked(long key, char *buf, int len);
int des_decrypt_checked(long key, char *buf, int len);

/*
 * PKCS#7, con bloque de 8 bytes. Aun cuando la longitud ya es multiplo
 * de ocho, se agrega un bloque completo de padding (0x08 repetido).
 * des_pkcs7_pad reserva memoria: el llamador debe usar free(*output).
 * Retorno: 0 = exito, -1 = argumentos/longitud invalidos, -2 = sin memoria.
 */
int des_pkcs7_pad(const unsigned char *input, size_t input_len,
                  unsigned char **output, size_t *padded_len);

/* Valida PKCS#7 y devuelve la cantidad de bytes reales del mensaje. */
int des_pkcs7_unpad(const unsigned char *input, size_t padded_len,
                    size_t *message_len);

#endif
