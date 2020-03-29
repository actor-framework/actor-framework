#include <openssl/crypto.h>
#include <openssl/opensslv.h>

#include <cstdio>
#include <cstdlib>

int main() {
  long have = SSLeay();
  long want = OPENSSL_VERSION_NUMBER;
  printf("have OpenSSL %lx, want %lx\n", have, want);
  if (have == want)
    return EXIT_SUCCESS;
  return EXIT_FAILURE;
}
