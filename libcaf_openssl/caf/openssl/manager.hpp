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

#ifndef CAF_OPENSSL_MANAGER_HPP
#define CAF_OPENSSL_MANAGER_HPP

#include <set>
#include <string>

#include "caf/actor_system.hpp"
#include "caf/io/middleman_actor.hpp"

namespace caf {
namespace openssl {

/// Stores OpenSSL context information and provides access to necessary
/// credentials for establishing connections.
class manager : public actor_system::module {
public:
  ~manager() override;

  void start() override;

  void stop() override;

  void init(actor_system_config&) override;

  id_t id() const override;

  void* subtype_ptr() override;

  /// Returns an SSL-aware implementation of the middleman actor interface.
  inline const io::middleman_actor& actor_handle() const {
    return manager_;
  }

  /// Returns the enclosing actor system.
  inline actor_system& system() {
    return system_;
  }

  /// Returns true if configured to require certificate-based authentication
  /// of peers.
  bool authentication_enabled();

  /// Returns an OpenSSL manager using the default network backend.
  /// @warning Creating an OpenSSL manager will fail when using the ASIO
  ///          network backend or any other custom implementation.
  /// @throws `logic_error` if the middleman is not loaded or is not using the
  ///         default network backend.
  static actor_system::module* make(actor_system&, detail::type_list<>);

private:
  /// Private since instantiation is only allowed via `make`.
  manager(actor_system& sys);

  /// Reference to the parent.
  actor_system& system_;

  /// OpenSSL-aware connection manager.
  io::middleman_actor manager_;
};

} // namespace openssl
} // namespace caf

#endif // CAF_OPENSSL_MANAGER_HPP
