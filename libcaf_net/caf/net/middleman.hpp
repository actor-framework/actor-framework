// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/fwd.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/type_list.hpp"
#include "caf/version.hpp"

#include <thread>

namespace caf::net {

/// Provides a network backend for running protocol stacks.
class CAF_NET_EXPORT middleman : public actor_system_module {
public:
  // -- constants --------------------------------------------------------------

  /// Identifies the network manager module.
  actor_system_module::id_t id_v = actor_system_module::network_manager;

  // -- member types -----------------------------------------------------------

  using void_fun_t = void (*)();

  // -- static utility functions -----------------------------------------------

  static void init_global_meta_objects();

  /// Initializes global state for the network backend by calling
  /// platform-dependent functions such as `WSAStartup` and `ssl::startup()`.
  /// @returns a guard object shutting down the global state.
  static actor_system::global_state_guard init_host_system();

  // -- constructors, destructors, and assignment operators --------------------

  explicit middleman(actor_system& sys);

  ~middleman() override;

  // -- interface functions ----------------------------------------------------

  void start() override;

  void stop() override;

  void init(actor_system_config&) override;

  id_t id() const override;

  void* subtype_ptr() override;

  // -- factory functions ------------------------------------------------------

  /// Adds module-specific options to the config before loading the module.
  static void add_module_options(actor_system_config& cfg);

  /// Creates a new middleman instance.
  static actor_system_module* make(actor_system& sys);

  /// Checks whether the ABI of the middleman is compatible with the CAF core.
  /// Otherwise, calls `abort`.
  static void check_abi_compatibility(version::abi_token token);

  // -- properties -------------------------------------------------------------

  actor_system& system() {
    return sys_;
  }

  const actor_system_config& config() const noexcept {
    return sys_.config();
  }

  multiplexer& mpx() noexcept {
    return *mpx_;
  }

  const multiplexer& mpx() const noexcept {
    return *mpx_;
  }

  multiplexer* mpx_ptr() const noexcept {
    return mpx_.get();
  }

private:
  // -- member variables -------------------------------------------------------

  /// Points to the parent system.
  actor_system& sys_;

  /// Stores the global socket I/O multiplexer.
  multiplexer_ptr mpx_;

  /// Runs the multiplexer's event loop
  std::thread mpx_thread_;
};

} // namespace caf::net
