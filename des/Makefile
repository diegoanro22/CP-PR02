# Makefile del modulo DES.
CC = gcc
# Seleccion automatica: si libtirpc no implementa ecb_crypt, usa OpenSSL.
# Tambien se puede forzar: make BACKEND=openssl (o BACKEND=tirpc).
TIRPC_DES_OK := $(shell printf '%s\n' '#include <rpc/des_crypt.h>' 'int main(void) { char k[8]={0}, b[8]={0}; des_setparity(k); return ecb_crypt(k,b,8,DES_ENCRYPT|DES_SW); }' | $(CC) -x c - -o /dev/null -I/usr/include/tirpc -ltirpc >/dev/null 2>&1 && echo si || echo no)
BACKEND ?= $(if $(filter si,$(TIRPC_DES_OK)),tirpc,openssl)
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
ifeq ($(BACKEND),tirpc)
CFLAGS += -I/usr/include/tirpc
LDLIBS = -ltirpc
else ifeq ($(BACKEND),openssl)
CFLAGS += -DDES_USE_OPENSSL -Wno-deprecated-declarations
LDLIBS = -lcrypto
else
$(error BACKEND invalido: solo tirpc u openssl)
endif

.PHONY: all test clean
all: encrypt_file decrypt_file test_des_crypto

%.o: %.c des_crypto.h
	$(CC) $(CFLAGS) -c $< -o $@

encrypt_file: encrypt_file.o des_crypto.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

decrypt_file: decrypt_file.o des_crypto.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

test_des_crypto: test_des_crypto.o des_crypto.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

test: test_des_crypto
	./test_des_crypto

clean:
	rm -f encrypt_file decrypt_file test_des_crypto *.o ejemplo_cifrado.des ejemplo_restaurado.txt
