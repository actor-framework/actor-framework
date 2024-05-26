// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/io/middleman_actor.hpp"

#include "caf/actor_system.hpp"
#include "caf/detail/openssl_export.hpp"
#include "caf/version.hpp"

#include <set>
#include <string>

namespace caf::openssl {

/// Stores OpenSSL context information and provides access to necessary
/// credentials for establishing connections.
class CAF_OPENSSL_EXPORT manager : public actor_system_module {
public:
  ~manager() override;

  void start() override;

  void stop() override;

  void init(actor_system_config&) override;

  id_t id() const override;

  void* subtype_ptr() override;

  /// Returns an SSL-aware implementation of the middleman actor interface.
  const io::middleman_actor& actor_handle() const {
    return manager_;
  }

  /// Returns the enclosing actor system.
  actor_system& system() {
    return system_;
  }

  /// Returns the system-wide configuration.
  const actor_system_config& config() const {
    return system_.config();
  }

  /// Returns true if configured to require certificate-based authentication
  /// of peers.
  bool authentication_enabled();

  /// Adds module-specific options to the config before loading the module.
  static void add_module_options(actor_system_config& cfg);

  /// Returns an OpenSSL manager using the default network backend.
  /// @warning Creating an OpenSSL manager will fail when using
  //           a custom implementation.
  /// @throws `logic_error` if the middleman is not loaded or is not using the
  ///         default network backend.
  static actor_system_module* make(actor_system&);

  /// Checks whether the ABI of the middleman is compatible with the CAF core.
  /// Otherwise, calls `abort`.
  static void check_abi_compatibility(version::abi_token token);

  /// Adds message types of the OpenSSL module to the global meta object table.
  static void init_global_meta_objects();

private:
  /// Private since instantiation is only allowed via `make`.
  manager(actor_system& sys);

  /// Reference to the parent.
  actor_system& system_;

  /// OpenSSL-aware connection manager.
  io::middleman_actor manager_;
};

} // namespace caf::openssl
