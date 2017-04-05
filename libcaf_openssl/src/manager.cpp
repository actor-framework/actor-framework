/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "caf/expected.hpp"
#include "caf/actor_system.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_system_config.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#include "caf/openssl/middleman_actor.hpp"

namespace caf {
namespace openssl {

manager::~manager() {
  // nop
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
  if (system().config().middleman_detach_utility_actors)
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
    if (!system().config().openssl_certificate.size())
      CAF_RAISE_ERROR("No certificate configured for SSL endpoint");

    if (!system().config().openssl_key.size())
      CAF_RAISE_ERROR("No private key configured for SSL endpoint");
  }
}

actor_system::module::id_t manager::id() const {
  return openssl_manager;
}

void* manager::subtype_ptr() {
  return this;
}

bool manager::authentication_enabled() {
  auto& cfg = system().config();
  return cfg.openssl_certificate.size() || cfg.openssl_key.size()
         || cfg.openssl_passphrase.size() || cfg.openssl_capath.size()
         || cfg.openssl_cafile.size();
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
