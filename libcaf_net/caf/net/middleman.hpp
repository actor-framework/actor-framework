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

#include <set>
#include <string>
#include <thread>

#include "caf/actor_system.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/middleman_backend.hpp"
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

  ~middleman() override;

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

  // -- remoting ---------------------------------------------------------------

  expected<endpoint_manager_ptr> connect(const uri& locator);

  // Publishes an actor.
  template <class Handle>
  void publish(Handle whom, const std::string& path) {
    system().registry().put(path, whom);
  }

  /// Resolves a path to a remote actor.
  void resolve(const uri& locator, const actor& listener);

  template <class Handle = actor, class Rep, class Period>
  expected<Handle>
  remote_actor(const uri& locator,
               std::chrono::duration<Rep, Period> timeout_duration
               = std::chrono::seconds(5)) {
    scoped_actor self{sys_};
    resolve(locator, self);
    Handle handle;
    error err;
    self->receive(
      [&handle](strong_actor_ptr& ptr, const std::set<std::string>&) {
        handle = actor_cast<Handle>(std::move(ptr));
      },
      [&err](const error& e) { err = e; },
      after(timeout_duration) >>
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

  const multiplexer_ptr& mpx() const noexcept {
    return mpx_;
  }

  middleman_backend* backend(string_view scheme) const noexcept;

  expected<uint16_t> port(string_view scheme) const;

private:
  // -- constructors, destructors, and assignment operators --------------------

  explicit middleman(actor_system& sys);

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
  multiplexer_ptr mpx_;

  /// Stores all available backends for managing peers.
  middleman_backend_list backends_;

  /// Runs the multiplexer's event loop
  std::thread mpx_thread_;
};

} // namespace caf::net
