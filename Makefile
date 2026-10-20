# Compila el buscador MPI junto con el modulo DES de des/.
# Igual que des/Makefile: usa libtirpc si implementa ecb_crypt, si no OpenSSL.
# Se puede forzar: make BACKEND=openssl (o BACKEND=tirpc). TEST_EVERY=K tambien.
MPICC = mpicc
TIRPC_DES_OK := $(shell printf '%s\n' '#include <rpc/des_crypt.h>' 'int main(void) { char k[8]={0}, b[8]={0}; des_setparity(k); return ecb_crypt(k,b,8,DES_ENCRYPT|DES_SW); }' | $(CC) -x c - -o /dev/null -I/usr/include/tirpc -ltirpc >/dev/null 2>&1 && echo si || echo no)
BACKEND ?= $(if $(filter si,$(TIRPC_DES_OK)),tirpc,openssl)
TEST_EVERY ?= 1
CFLAGS = -std=c11 -O2 -Wall -Wextra -Ides -DTEST_EVERY=$(TEST_EVERY)
ifeq ($(BACKEND),tirpc)
CFLAGS += -I/usr/include/tirpc
LDLIBS = -ltirpc
else ifeq ($(BACKEND),openssl)
CFLAGS += -DDES_USE_OPENSSL -Wno-deprecated-declarations
LDLIBS = -lcrypto
else
$(error BACKEND invalido: solo tirpc u openssl)
endif

.PHONY: all des clean
all: bruteforce des

bruteforce: bruteforce_.c des/des_crypto.c des/des_crypto.h
	$(MPICC) $(CFLAGS) bruteforce_.c des/des_crypto.c -o $@ $(LDLIBS)

des:
	$(MAKE) -C des BACKEND=$(BACKEND) all

clean:
	rm -f bruteforce
	$(MAKE) -C des clean
