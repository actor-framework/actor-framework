// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <chrono>
#include <set>
#include <string>
#include <thread>

#include "caf/actor_system.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/net/connection_acceptor.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/scoped_actor.hpp"

namespace caf::net {

class CAF_NET_EXPORT middleman : public actor_system::module {
public:
  // -- member types -----------------------------------------------------------

  using module = actor_system::module;

  using module_ptr = actor_system::module_ptr;

  // -- static utility functions -----------------------------------------------

  static void init_global_meta_objects();

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

  static module* make(actor_system& sys, detail::type_list<>);

  /// Adds module-specific options to the config before loading the module.
  static void add_module_options(actor_system_config& cfg);

  // -- properties -------------------------------------------------------------

  actor_system& system() {
    return sys_;
  }

  const actor_system_config& config() const noexcept {
    return sys_.config();
  }

  multiplexer& mpx() noexcept {
    return mpx_;
  }

  const multiplexer& mpx() const noexcept {
    return mpx_;
  }

  multiplexer* mpx_ptr() noexcept {
    return &mpx_;
  }

  const multiplexer* mpx_ptr() const noexcept {
    return &mpx_;
  }

private:
  // -- member variables -------------------------------------------------------

  /// Points to the parent system.
  actor_system& sys_;

  /// Stores the global socket I/O multiplexer.
  multiplexer mpx_;

  /// Runs the multiplexer's event loop
  std::thread mpx_thread_;
};

} // namespace caf::net
