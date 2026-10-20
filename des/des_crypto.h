#ifndef DES_CRYPTO_H
#define DES_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#define DES_BLOCK_BYTES 8U
#define DES_EFFECTIVE_KEY_BITS 56U
#define DES_EFFECTIVE_KEY_COUNT (UINT64_C(1) << DES_EFFECTIVE_KEY_BITS)

/* Convierte una llave de 56 bits a los 8 bytes de DES. Devuelve 0 o -1. */
int des_key_to_bytes(long key, unsigned char out[DES_BLOCK_BYTES]);

/* Cifran o descifran len bytes en el mismo buffer; len debe ser multiplo de 8. */
void encrypt(long key, char *buf, int len);
void decrypt(long key, char *buf, int len);

/* Igual que las anteriores, pero devuelven un codigo de error en vez de abortar. */
int des_encrypt_checked(long key, char *buf, int len);
int des_decrypt_checked(long key, char *buf, int len);

/* Padding PKCS#7 de 8 bytes. *output se libera con free. Devuelve 0, -1 o -2 (sin memoria). */
int des_pkcs7_pad(const unsigned char *input, size_t input_len,
                  unsigned char **output, size_t *padded_len);

/* Valida el padding y devuelve en message_len la longitud sin padding. */
int des_pkcs7_unpad(const unsigned char *input, size_t padded_len,
                    size_t *message_len);

#endif
