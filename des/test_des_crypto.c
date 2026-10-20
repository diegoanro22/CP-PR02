/* Pruebas del modulo DES */
#include "des_crypto.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int set_bits(unsigned char byte) {
    unsigned int count = 0;
    for (unsigned int i = 0; i < 8; ++i) count += (byte >> i) & 1U;
    return count;
}

static void test_key_conversion(void) {
    const long candidates[] = {0L, 1L, 123456L, 1048576L,
                               18014398509481984L, 72057594037927935L};
    for (size_t j = 0; j < sizeof candidates / sizeof candidates[0]; ++j) {
        unsigned char bytes[DES_BLOCK_BYTES];
        assert(des_key_to_bytes(candidates[j], bytes) == 0);
        uint64_t recovered = 0;
        for (size_t i = 0; i < DES_BLOCK_BYTES; ++i) {
            assert(set_bits(bytes[i]) % 2U == 1U);  /* paridad impar */
            recovered |= (uint64_t)(bytes[i] >> 1) << (7U * i);
        }
        assert(recovered == (uint64_t)candidates[j]);
    }
    unsigned char bytes[DES_BLOCK_BYTES];
    assert(des_key_to_bytes(-1L, bytes) != 0);
    assert(des_key_to_bytes(72057594037927936L, bytes) != 0);
}

/* Vector conocido de DES (FIPS): K=133457799BBCDFF1, P=0123456789ABCDEF. */
static void test_standard_vector(void) {
    const unsigned char standard_key[8] =
        {0x13, 0x34, 0x57, 0x79, 0x9b, 0xbc, 0xdf, 0xf1};
    const unsigned char standard_plain[8] =
        {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    const unsigned char expected_cipher[8] =
        {0x85, 0xe8, 0x13, 0x54, 0x0f, 0x0a, 0xb4, 0x05};
    uint64_t key_as_number = 0;
    for (unsigned int i = 0; i < 8; ++i) {
        key_as_number |= (uint64_t)(standard_key[i] >> 1) << (7U * i);
    }
    unsigned char buffer[8];
    memcpy(buffer, standard_plain, sizeof buffer);
    encrypt((long)key_as_number, (char *)buffer, (int)sizeof buffer);
    assert(memcmp(buffer, expected_cipher, sizeof buffer) == 0);
    decrypt((long)key_as_number, (char *)buffer, (int)sizeof buffer);
    assert(memcmp(buffer, standard_plain, sizeof buffer) == 0);
}

static void test_roundtrip(void) {
    const long candidates[] = {0L, 1L, 123456L, 1048576L,
                               72057594037927935L};
    const size_t lengths[] = {0U, 1U, 7U, 8U, 15U, 16U, 53U, 8192U, 8199U, 8208U};
    for (size_t j = 0; j < sizeof candidates / sizeof candidates[0]; ++j) {
        for (size_t k = 0; k < sizeof lengths / sizeof lengths[0]; ++k) {
            size_t len = lengths[k];
            unsigned char *input = malloc(len ? len : 1U);
            assert(input != NULL);
            for (size_t i = 0; i < len; ++i) input[i] = (unsigned char)(i * 31U + 19U);
            unsigned char *padded = NULL;
            size_t plen = 0;
            assert(des_pkcs7_pad(input, len, &padded, &plen) == 0);
            assert(plen % 8 == 0 && plen > len);
            unsigned char *original = malloc(plen);
            assert(original != NULL);
            memcpy(original, padded, plen);
            encrypt(candidates[j], (char *)padded, (int)plen);
            assert(memcmp(padded, original, plen) != 0);
            decrypt(candidates[j], (char *)padded, (int)plen);
            assert(memcmp(padded, original, plen) == 0);
            size_t recovered_length = 0;
            assert(des_pkcs7_unpad(padded, plen, &recovered_length) == 0);
            assert(recovered_length == len && memcmp(padded, input, len) == 0);
            free(original);
            free(padded);
            free(input);
        }
    }
}

static void test_search_phrase(void) {
    /* con la llave incorrecta la frase no debe aparecer */
    const char message[] =
        "Este mensaje contiene los mismos del proyecto 1 para nuestra prueba.";
    const char search[] = " los mismos del proyecto 1 ";
    unsigned char *padded = NULL;
    size_t len = 0;
    assert(des_pkcs7_pad((const unsigned char *)message, strlen(message),
                         &padded, &len) == 0);
    encrypt(123456L, (char *)padded, (int)len);
    unsigned char *wrong = malloc(len + 1);
    assert(wrong != NULL);
    memcpy(wrong, padded, len);
    wrong[len] = 0;
    decrypt(0L, (char *)wrong, (int)len);
    assert(strstr((const char *)wrong, search) == NULL);
    free(wrong);
    decrypt(123456L, (char *)padded, (int)len);
    size_t actual = 0;
    assert(des_pkcs7_unpad(padded, len, &actual) == 0);
    assert(actual == strlen(message));
    assert(memcmp(padded, message, actual) == 0);
    free(padded);
}

static void test_errors(void) {
    char unaligned[9] = {0};
    assert(des_encrypt_checked(1L, unaligned, 9) != 0);
    assert(des_decrypt_checked(-1L, unaligned, 8) != 0);
    unsigned char bad_pad[8] = {0, 0, 0, 0, 0, 0, 0, 0x02};
    size_t real_len = 0;
    assert(des_pkcs7_unpad(bad_pad, sizeof bad_pad, &real_len) != 0);
}

int main(void) {
    test_key_conversion();
    test_standard_vector();
    test_roundtrip();
    test_search_phrase();
    test_errors();
    puts("OK: paridad DES, vector estandar, PKCS#7, cifrado completo y descifrado.");
    return 0;
}
