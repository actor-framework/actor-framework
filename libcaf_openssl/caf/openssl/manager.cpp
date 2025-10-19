// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/openssl/manager.hpp"

#include "caf/version.hpp"

CAF_PUSH_WARNINGS
#include <openssl/err.h>
#include <openssl/ssl.h>
CAF_POP_WARNINGS

#include "caf/io/basp_broker.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#include "caf/openssl/middleman_actor.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/expected.hpp"
#include "caf/log/openssl.hpp"
#include "caf/raise_error.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/version.hpp"

#include <mutex>
#include <vector>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
struct CRYPTO_dynlock_value {
  std::mutex mtx;
};

namespace {

int init_count = 0;

std::mutex init_mutex;

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

namespace caf::openssl {

namespace {

bool is_nonempty_string(const actor_system_config& cfg, std::string_view key) {
  if (auto str = get_if<std::string>(&cfg, key))
    return !str->empty();
  return false;
}

} // namespace

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
  auto lg = log::openssl::trace("");
  manager_ = make_middleman_actor(
    system(), system().middleman().named_broker<io::basp_broker>("BASP"));
}

void manager::stop() {
  auto lg = log::openssl::trace("");
  scoped_actor self{system(), true};
  self->send_exit(manager_, exit_reason::kill);
  if (!get_or(config(), "caf.middleman.attach-utility-actors", false))
    self->wait_for(manager_);
  manager_ = nullptr;
}

void manager::init(actor_system_config&) {
  ERR_load_crypto_strings();
  OPENSSL_add_all_algorithms_conf();
  SSL_library_init();
  SSL_load_error_strings();
  if (authentication_enabled()) {
    auto& cfg = system().config();
    if (!is_nonempty_string(cfg, "caf.openssl.certificate"))
      CAF_RAISE_ERROR("No certificate configured for SSL endpoint");
    if (!is_nonempty_string(cfg, "caf.openssl.key"))
      CAF_RAISE_ERROR("No private key configured for SSL endpoint");
  }
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  std::lock_guard<std::mutex> lock{init_mutex};
  ++init_count;
  if (init_count == 1) {
    mutexes = std::vector<std::mutex>{static_cast<size_t>(CRYPTO_num_locks())};
    CRYPTO_set_locking_callback(locking_function);
    CRYPTO_set_dynlock_create_callback(dynlock_create);
    CRYPTO_set_dynlock_lock_callback(dynlock_lock);
    CRYPTO_set_dynlock_destroy_callback(dynlock_destroy);
    // OpenSSL's default thread ID callback should work, so don't set our own.
  }
#endif
}

actor_system_module::id_t manager::id() const {
  return openssl_manager;
}

void* manager::subtype_ptr() {
  return this;
}

bool manager::authentication_enabled() {
  auto& cfg = system().config();
  return is_nonempty_string(cfg, "caf.openssl.certificate")
         || is_nonempty_string(cfg, "caf.openssl.key")
         || is_nonempty_string(cfg, "caf.openssl.passphrase")
         || is_nonempty_string(cfg, "caf.openssl.capath")
         || is_nonempty_string(cfg, "caf.openssl.cafile");
}

void manager::add_module_options(actor_system_config& cfg) {
  // Add options to the CLI parser.
  config_option_adder(cfg.custom_options(), "caf.openssl")
    .add<std::string>("certificate",
                      "path to the PEM-formatted certificate file")
    .add<std::string>("key", "path to the private key file for this node")
    .add<std::string>("passphrase", "passphrase to decrypt the private key")
    .add<std::string>(
      "capath", "path to an OpenSSL-style directory of trusted certificates")
    .add<std::string>(
      "cafile", "path to a file of concatenated PEM-formatted certificates")
    .add<std::string>("cipher-list",
                      "colon-separated list of OpenSSL cipher strings to use");
}

actor_system_module* manager::make(actor_system& sys) {
  if (!sys.has_middleman())
    CAF_RAISE_ERROR("Cannot start OpenSSL module without middleman.");
  auto ptr = &sys.middleman().backend();
  if (dynamic_cast<io::network::default_multiplexer*>(ptr) == nullptr)
    CAF_RAISE_ERROR("Cannot start OpenSSL module without default backend.");
  return new manager(sys);
}

void manager::check_abi_compatibility(version::abi_token token) {
  version::check_abi_compatibility(token);
}

void manager::init_global_meta_objects() {
  // nop
}

manager::manager(actor_system& sys) : system_(sys) {
  // nop
}

} // namespace caf::openssl
