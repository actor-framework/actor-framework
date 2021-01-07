/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

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
#include "caf/net/middleman_backend.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/scoped_actor.hpp"

namespace caf::net {

class CAF_NET_EXPORT middleman : public actor_system::module {
public:
  // -- member types -----------------------------------------------------------

  using module = actor_system::module;

  using module_ptr = actor_system::module_ptr;

  using middleman_backend_list = std::vector<middleman_backend_ptr>;

  // -- static utility functions -----------------------------------------------

  static void init_global_meta_objects();

  // -- constructors, destructors, and assignment operators --------------------

  explicit middleman(actor_system& sys);

  ~middleman() override;

  // -- socket manager functions -----------------------------------------------

  ///
  /// @param sock An accept socket in listening mode. For a TCP socket, this
  ///             socket must already listen to an address plus port.
  /// @param factory An application stack factory.
  template <class Socket, class Factory>
  auto make_acceptor(Socket sock, Factory factory) {
    using connected_socket_type = typename Socket::connected_socket_type;
    if constexpr (detail::is_callable_with<Factory, connected_socket_type,
                                           multiplexer*>::value) {
      connection_acceptor_factory_adapter<Factory> adapter{std::move(factory)};
      return make_acceptor(std::move(sock), std::move(adapter));
    } else {
      using impl = connection_acceptor<Socket, Factory>;
      auto ptr = make_socket_manager<impl>(std::move(sock), &mpx_,
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

  template <class... Ts>
  static module* make(actor_system& sys, detail::type_list<Ts...> token) {
    std::unique_ptr<middleman> result{new middleman(sys)};
    if (sizeof...(Ts) > 0) {
      result->backends_.reserve(sizeof...(Ts));
      create_backends(*result, token);
    }
    return result.release();
  }

  /// Adds module-specific options to the config before loading the module.
  static void add_module_options(actor_system_config& cfg);

  // -- remoting ---------------------------------------------------------------

  expected<endpoint_manager_ptr> connect(const uri& locator);

  // Publishes an actor.
  template <class Handle>
  void publish(Handle whom, const std::string& path) {
    // TODO: Currently, we can't get the interface from the registry. Either we
    // change that, or we need to associate the handle with the interface.
    system().registry().put(path, whom);
  }

  /// Resolves a path to a remote actor.
  void resolve(const uri& locator, const actor& listener);

  template <class Handle = actor, class Duration = std::chrono::seconds>
  expected<Handle>
  remote_actor(const uri& locator, Duration timeout = std::chrono::seconds(5)) {
    scoped_actor self{sys_};
    resolve(locator, self);
    Handle handle;
    error err;
    self->receive(
      [&handle](strong_actor_ptr& ptr, const std::set<std::string>&) {
        // TODO: This cast is not type-safe.
        handle = actor_cast<Handle>(std::move(ptr));
      },
      [&err](const error& e) { err = e; },
      after(timeout) >>
        [&err] {
          err = make_error(sec::runtime_error,
                           "manager did not respond with a proxy.");
        });
    if (err)
      return err;
    if (handle)
      return handle;
    return make_error(sec::runtime_error, "cast to handle-type failed");
  }

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

  middleman_backend* backend(string_view scheme) const noexcept;

  expected<uint16_t> port(string_view scheme) const;

private:
  // -- utility functions ------------------------------------------------------

  static void create_backends(middleman&, detail::type_list<>) {
    // End of recursion.
  }

  template <class T, class... Ts>
  static void create_backends(middleman& mm, detail::type_list<T, Ts...>) {
    mm.backends_.emplace_back(new T(mm));
    create_backends(mm, detail::type_list<Ts...>{});
  }

  // -- member variables -------------------------------------------------------

  /// Points to the parent system.
  actor_system& sys_;

  /// Stores the global socket I/O multiplexer.
  multiplexer mpx_;

  /// Stores all available backends for managing peers.
  middleman_backend_list backends_;

  /// Runs the multiplexer's event loop
  std::thread mpx_thread_;
};

} // namespace caf::net
