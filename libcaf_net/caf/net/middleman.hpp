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

  // -- socket manager functions -----------------------------------------------

  /// Creates a new acceptor that accepts incoming connections from @p sock and
  /// creates socket managers using @p factory.
  /// @param sock An accept socket in listening mode. For a TCP socket, this
  ///             socket must already listen to an address plus port.
  /// @param factory A function object for creating socket managers that take
  ///                ownership of incoming connections.
  /// @param limit The maximum number of connections that this acceptor should
  ///              establish or 0 for 'no limit'.
  template <class Socket, class Factory>
  auto make_acceptor(Socket sock, Factory factory, size_t limit = 0) {
    using connected_socket_type = typename Socket::connected_socket_type;
    if constexpr (detail::is_callable_with<Factory, connected_socket_type,
                                           multiplexer*>::value) {
      connection_acceptor_factory_adapter<Factory> adapter{std::move(factory)};
      return make_acceptor(std::move(sock), std::move(adapter), limit);
    } else {
      using impl = connection_acceptor<Socket, Factory>;
      auto ptr = make_socket_manager<impl>(std::move(sock), &mpx_, limit,
                                           std::move(factory));
      mpx_.init(ptr);
      return ptr;
    }
  }

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
