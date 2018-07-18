/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/openssl/manager.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

#include <vector>
#include <mutex>

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/expected.hpp"
#include "caf/raise_error.hpp"
#include "caf/scoped_actor.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#include "caf/openssl/middleman_actor.hpp"

struct CRYPTO_dynlock_value {
  std::mutex mtx;
};

namespace caf {
namespace openssl {

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static int init_count = 0;
static std::mutex init_mutex;
static std::vector<std::mutex> mutexes;

static void locking_function(int mode, int n,
                             const char* /* file */, int /* line */) {
  if (mode & CRYPTO_LOCK)
    mutexes[n].lock();
  else
    mutexes[n].unlock();
}

static CRYPTO_dynlock_value* dynlock_create(const char* /* file */,
                                            int /* line */) {
  return new CRYPTO_dynlock_value{};
}

static void dynlock_lock(int mode, CRYPTO_dynlock_value* dynlock,
                         const char* /* file */, int /* line */) {
  if (mode & CRYPTO_LOCK)
    dynlock->mtx.lock();
  else
    dynlock->mtx.unlock();
}

static void dynlock_destroy(CRYPTO_dynlock_value* dynlock,
                            const char* /* file */, int /* line */) {
  delete dynlock;
}
#endif

manager::~manager() {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  std::lock_guard<std::mutex> lock{init_mutex};
  --init_count;
  if (init_count == 0) {
    CRYPTO_set_locking_callback(nullptr);
    CRYPTO_set_dynlock_create_callback(nullptr);
    CRYPTO_set_dynlock_lock_callback(nullptr);
    CRYPTO_set_dynlock_destroy_callback(nullptr);
    mutexes = std::vector<std::mutex>(0);
  }
#endif
}

void manager::start() {
  CAF_LOG_TRACE("");
  manager_ = make_middleman_actor(
    system(), system().middleman().named_broker<io::basp_broker>(atom("BASP")));
}

void manager::stop() {
  CAF_LOG_TRACE("");
  scoped_actor self{system(), true};
  self->send_exit(manager_, exit_reason::kill);
  if (!get_or(config(), "middleman.attach-utility-actors", false))
    self->wait_for(manager_);
  manager_ = nullptr;
}

void manager::init(actor_system_config&) {
  CAF_LOG_TRACE("");
  ERR_load_crypto_strings();
  OPENSSL_add_all_algorithms_conf();
  SSL_library_init();
  SSL_load_error_strings();
  if (authentication_enabled()) {
    if (system().config().openssl_certificate.size() == 0)
      CAF_RAISE_ERROR("No certificate configured for SSL endpoint");
    if (system().config().openssl_key.size() == 0)
      CAF_RAISE_ERROR("No private key configured for SSL endpoint");
  }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  std::lock_guard<std::mutex> lock{init_mutex};
  ++init_count;
  if (init_count == 1) {
    mutexes = std::vector<std::mutex>(CRYPTO_num_locks());
    CRYPTO_set_locking_callback(locking_function);
    CRYPTO_set_dynlock_create_callback(dynlock_create);
    CRYPTO_set_dynlock_lock_callback(dynlock_lock);
    CRYPTO_set_dynlock_destroy_callback(dynlock_destroy);
    // OpenSSL's default thread ID callback should work, so don't set our own.
  }
#endif
}

actor_system::module::id_t manager::id() const {
  return openssl_manager;
}

void* manager::subtype_ptr() {
  return this;
}

bool manager::authentication_enabled() {
  auto& cfg = system().config();
  return cfg.openssl_certificate.size() > 0 || cfg.openssl_key.size() > 0
         || cfg.openssl_passphrase.size() > 0 || cfg.openssl_capath.size() > 0
         || cfg.openssl_cafile.size() > 0;
}

actor_system::module* manager::make(actor_system& sys, detail::type_list<>) {
  if (!sys.has_middleman())
    CAF_RAISE_ERROR("Cannot start OpenSSL module without middleman.");
  auto ptr = &sys.middleman().backend();
  if (dynamic_cast<io::network::default_multiplexer*>(ptr) == nullptr)
    CAF_RAISE_ERROR("Cannot start OpenSSL module without default backend.");
  return new manager(sys);
}

manager::manager(actor_system& sys) : system_(sys) {
  // nop
}

} // namespace openssl
} // namespace caf
