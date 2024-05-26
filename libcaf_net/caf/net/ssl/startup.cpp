// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/ssl/startup.hpp"

#include "caf/config.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

#if OPENSSL_VERSION_NUMBER < 0x10100000L

#  include <mutex>
#  include <vector>

struct CRYPTO_dynlock_value {
  std::mutex mtx;
};

namespace {

std::vector<std::mutex> mutexes;

void locking_function(int mode, int n, const char*, int) {
  if (mode & CRYPTO_LOCK)
    mutexes[static_cast<size_t>(n)].lock();
  else
    mutexes[static_cast<size_t>(n)].unlock();
}

CRYPTO_dynlock_value* dynlock_create(const char*, int) {
  return new CRYPTO_dynlock_value;
}

void dynlock_lock(int mode, CRYPTO_dynlock_value* dynlock, const char*, int) {
  if (mode & CRYPTO_LOCK)
    dynlock->mtx.lock();
  else
    dynlock->mtx.unlock();
}

void dynlock_destroy(CRYPTO_dynlock_value* dynlock, const char*, int) {
  delete dynlock;
}

} // namespace

#endif

namespace caf::net::ssl {

void startup() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
  mutexes = std::vector<std::mutex>{static_cast<size_t>(CRYPTO_num_locks())};
  CRYPTO_set_locking_callback(locking_function);
  CRYPTO_set_dynlock_create_callback(dynlock_create);
  CRYPTO_set_dynlock_lock_callback(dynlock_lock);
  CRYPTO_set_dynlock_destroy_callback(dynlock_destroy);
#else
  OPENSSL_init_ssl(0, nullptr);
#endif
}

void cleanup() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  CRYPTO_set_locking_callback(nullptr);
  CRYPTO_set_dynlock_create_callback(nullptr);
  CRYPTO_set_dynlock_lock_callback(nullptr);
  CRYPTO_set_dynlock_destroy_callback(nullptr);
  mutexes.clear();
#else
  ERR_free_strings();
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
#endif
}

} // namespace caf::net::ssl
