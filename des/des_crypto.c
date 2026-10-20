/* DES-ECB con libtirpc (ecb_crypt) u OpenSSL */
#include "des_crypto.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(DES_USE_OPENSSL)
#include <openssl/des.h>
#else
#include <rpc/des_crypt.h>
#endif

#if LONG_MAX < 72057594037927935LL
#error "Este proyecto requiere un entorno de 64 bits (long >= 56 bits)."
#endif

int des_key_to_bytes(long key, unsigned char out[DES_BLOCK_BYTES]) {
    if (out == NULL || key < 0 || (uint64_t)key >= DES_EFFECTIVE_KEY_COUNT) {
        return -1;
    }

    /* 7 bits de la llave en los bits altos de cada byte; el bit 0 es la paridad */
    uint64_t remaining = (uint64_t)key;
    for (size_t i = 0; i < DES_BLOCK_BYTES; ++i) {
        out[i] = (unsigned char)((remaining & UINT64_C(0x7F)) << 1);
        remaining >>= 7;
    }
#if defined(DES_USE_OPENSSL)
    DES_set_odd_parity((DES_cblock *)out);
#else
    des_setparity((char *)out);
#endif
    return 0;
}

static int des_transform(long key, char *buf, int len, int encrypting) {
    unsigned char des_key[DES_BLOCK_BYTES];
    if (buf == NULL || len <= 0 || (len % (int)DES_BLOCK_BYTES) != 0 ||
        des_key_to_bytes(key, des_key) != 0) {
        return -1;
    }

    size_t processed = 0;
    const size_t total = (size_t)len;
#if defined(DES_USE_OPENSSL)
    DES_key_schedule schedule;
    DES_cblock openssl_key;
    memcpy(openssl_key, des_key, DES_BLOCK_BYTES);
    DES_set_key_unchecked(&openssl_key, &schedule);
    while (processed < total) {
        DES_cblock block;
        memcpy(block, buf + processed, DES_BLOCK_BYTES);
        DES_ecb_encrypt(&block, &block, &schedule,
                        encrypting ? DES_ENCRYPT : DES_DECRYPT);
        memcpy(buf + processed, block, DES_BLOCK_BYTES);
        processed += DES_BLOCK_BYTES;
    }
#else
    /* ecb_crypt procesa como maximo DES_MAXDATA bytes por llamada */
    while (processed < total) {
        size_t remain = total - processed;
        unsigned int chunk = (unsigned int)(remain > DES_MAXDATA
                                ? DES_MAXDATA : remain);
        int status = ecb_crypt((char *)des_key, buf + processed, chunk,
                       (encrypting ? DES_ENCRYPT : DES_DECRYPT) | DES_SW);
        if (DES_FAILED(status)) {
            return -2;
        }
        processed += chunk;
    }
#endif
    return 0;
}

int des_encrypt_checked(long key, char *buf, int len) {
    return des_transform(key, buf, len, 1);
}

int des_decrypt_checked(long key, char *buf, int len) {
    return des_transform(key, buf, len, 0);
}

void encrypt(long key, char *buf, int len) {
    int result = des_encrypt_checked(key, buf, len);
    if (result != 0) {
        fprintf(stderr, "Error en encrypt: llave, longitud o cifrado invalido (codigo %d).\n", result);
        abort();
    }
}

void decrypt(long key, char *buf, int len) {
    int result = des_decrypt_checked(key, buf, len);
    if (result != 0) {
        fprintf(stderr, "Error en decrypt: llave, longitud o cifrado invalido (codigo %d).\n", result);
        abort();
    }
}

int des_pkcs7_pad(const unsigned char *input, size_t input_len,
                  unsigned char **output, size_t *padded_len) {
    if (output == NULL || padded_len == NULL ||
        (input == NULL && input_len != 0)) {
        return -1;
    }
    *output = NULL;
    *padded_len = 0;

    size_t pad_count = DES_BLOCK_BYTES - (input_len % DES_BLOCK_BYTES);
    if (input_len > SIZE_MAX - pad_count ||
        input_len + pad_count > (size_t)INT_MAX) {
        return -1;
    }
    size_t padded_size = input_len + pad_count;
    unsigned char *buffer = malloc(padded_size);
    if (buffer == NULL) {
        return -2;
    }
    if (input_len > 0) {
        memcpy(buffer, input, input_len);
    }
    memset(buffer + input_len, (int)pad_count, pad_count);
    *output = buffer;
    *padded_len = padded_size;
    return 0;
}

int des_pkcs7_unpad(const unsigned char *input, size_t padded_len,
                    size_t *message_len) {
    if (input == NULL || message_len == NULL || padded_len == 0 ||
        (padded_len % DES_BLOCK_BYTES) != 0) {
        return -1;
    }
    unsigned int count = input[padded_len - 1];
    if (count == 0 || count > DES_BLOCK_BYTES) {
        return -1;
    }
    for (unsigned int i = 0; i < count; ++i) {
        if (input[padded_len - 1 - i] != (unsigned char)count) {
            return -1;
        }
    }
    *message_len = padded_len - count;
    return 0;
}
