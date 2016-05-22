/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_IO_MIDDLEMAN_HPP
#define CAF_IO_MIDDLEMAN_HPP

#include <map>
#include <vector>
#include <memory>
#include <thread>

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/actor_system.hpp"
#include "caf/proxy_registry.hpp"

#include "caf/io/hook.hpp"
#include "caf/io/broker.hpp"
#include "caf/io/middleman_actor.hpp"
#include "caf/io/network/multiplexer.hpp"

namespace caf {
namespace io {

/// Manages brokers and network backends.
class middleman : public actor_system::module {
public:
  friend class actor_system;

  ~middleman();

  /// Publishes `whom` at `port`.
  /// @param whom Actor that should be published at `port`.
  /// @param port Unused TCP port.
  /// @param in The IP address to listen to or `INADDR_ANY` if `in == nullptr`.
  /// @returns The actual port the OS uses after `bind()`. If `port == 0` the OS
  ///          chooses a random high-level port.
  template <class Handle>
  uint16_t publish(Handle&& whom, uint16_t port,
                   const char* in = nullptr, bool reuse_addr = false) {
    detail::type_list<typename std::decay<Handle>::type> tk;
    return publish(actor_cast<strong_actor_ptr>(std::forward<Handle>(whom)),
                   system().message_types(tk), port, in, reuse_addr);
  }

  /// Makes *all* local groups accessible via network
  /// on address `addr` and `port`.
  /// @returns The actual port the OS uses after `bind()`. If `port == 0`
  ///          the OS chooses a random high-level port.
  uint16_t publish_local_groups(uint16_t port, const char* addr = nullptr);

  /// Unpublishes `whom` by closing `port` or all assigned ports if `port == 0`.
  /// @param whom Actor that should be unpublished at `port`.
  /// @param port TCP port.
  template <class Handle>
  void unpublish(const Handle& whom, uint16_t port = 0) {
    unpublish(whom.address(), port);
  }

  /// Establish a new connection to the typed actor at `host` on given `port`.
  /// @param host Valid hostname or IP address.
  /// @param port TCP port.
  /// @returns An {@link actor_ptr} to the proxy instance
  ///          representing a typed remote actor.
  /// @throws network_error Thrown on connection error or when connecting
  ///                       to an untyped otherwise unexpected actor.
  template <class ActorHandle>
  ActorHandle typed_remote_actor(std::string host, uint16_t port) {
    detail::type_list<ActorHandle> tk;
    auto x = remote_actor(system().message_types(tk), std::move(host), port);
    if (! x)
      throw std::runtime_error("received nullptr from middleman");
    return actor_cast<ActorHandle>(std::move(x));
  }

  /// Establish a new connection to the actor at `host` on given `port`.
  /// @param host Valid hostname or IP address.
  /// @param port TCP port.
  /// @returns An {@link actor_ptr} to the proxy instance
  ///          representing a remote actor.
  /// @throws network_error Thrown on connection error or
  ///                       when connecting to a typed actor.
  inline actor remote_actor(std::string host, uint16_t port) {
    return typed_remote_actor<actor>(std::move(host), port);
  }

  /// <group-name>@<host>:<port>
  group remote_group(const std::string& group_uri);

  group remote_group(const std::string& group_identifier,
                     const std::string& host, uint16_t port);

  /// Returns the enclosing actor system.
  inline actor_system& system() {
    return system_;
  }

  /// Returns a handle to the actor managing the middleman singleton.
  middleman_actor actor_handle();

  /// Returns the broker associated with `name` or creates a
  /// new instance of type `Impl`.
  template <class Impl>
  actor named_broker(atom_value name) {
    auto i = named_brokers_.find(name);
    if (i != named_brokers_.end())
      return i->second;
    actor_config cfg{&backend()};
    auto result = system().spawn_impl<Impl, hidden>(cfg);
    named_brokers_.emplace(name, result);
    return result;
  }

  /// Runs `fun` in the event loop of the middleman.
  /// @note This member function is thread-safe.
  template <class F>
  void run_later(F fun) {
    backend().post(fun);
  }

  /// Returns the IO backend used by this middleman.
  virtual network::multiplexer& backend() = 0;

  /// Invokes the callback(s) associated with given event.
  template <hook::event_type Event, typename... Ts>
  void notify(Ts&&... ts) {
    if (hooks_)
      hooks_->handle<Event>(std::forward<Ts>(ts)...);
  }

  /// Adds a new hook to the middleman.
  template<class C, typename... Ts>
  void add_hook(Ts&&... xs) {
    // if only we could move a unique_ptr into a lambda in C++11
    auto ptr = new C(system_, std::forward<Ts>(xs)...);
    backend().dispatch([=] {
      ptr->next.swap(hooks_);
      hooks_.reset(ptr);
    });
  }

  /// Returns whether this middleman has any hooks installed.
  inline bool has_hook() const {
    return hooks_ != nullptr;
  }

  /// Adds a function object to this middleman that is called
  /// when shutting down this middleman.
  template <class F>
  void add_shutdown_cb(F fun) {
    struct impl : hook {
      impl(actor_system&, F&& f) : fun_(std::move(f)) {
        // nop
      }

      void before_shutdown_cb() override {
        fun_();
        call_next<hook::before_shutdown>();
      }

      F fun_;
    };
    add_hook<impl>(std::move(fun));
  }

/*
  /// Returns the actor associated with `name` at `nid` or
  /// `invalid_actor` if `nid` is not connected or has no actor
  /// associated to this `name`.
  /// @warning Blocks the caller until `nid` responded to the lookup.
  actor remote_lookup(atom_value name, const node_id& nid);
*/

  /// Smart pointer for `network::multiplexer`.
  using backend_pointer = std::unique_ptr<network::multiplexer>;

  /// Used to initialize the backend during construction.
  using backend_factory = std::function<backend_pointer ()>;

  void start() override;

  void stop() override;

  void init(actor_system_config&) override;

  id_t id() const override;

  void* subtype_ptr() override;

  /// Spawns a new functor-based broker.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn_broker(F fun, Ts&&... xs) {
    actor_config cfg{&backend()};
    return system().spawn_functor<Os>(cfg, fun, std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based broker connected to `host:port` if
  /// a connection succeeds or the occurred error.
  /// @warning Blocks the caller for the duration of the connection process.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn_client(F fun, const std::string& host, uint16_t port, Ts&&... xs) {
    using impl = typename infer_handle_from_fun<F>::impl;
    return spawn_client_impl<Os, impl>(std::move(fun), host, port,
                                       std::forward<Ts>(xs)...);
  }

  /// Spawns a new broker as server running on given `port`.
  /// @warning Blocks the caller until the server socket is initialized.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn_server(F fun, uint16_t port, Ts&&... xs) {
    using impl = typename infer_handle_from_fun<F>::impl;
    return spawn_server_impl<Os, impl>(std::move(fun), port,
                                       std::forward<Ts>(xs)...);
  }

  /// Returns a middleman using the default network backend.
  static actor_system::module* make(actor_system&, detail::type_list<>);

  template <class Backend>
  static actor_system::module* make(actor_system& sys,
                                    detail::type_list<Backend>) {
    class impl : public middleman {
    public:
      impl(actor_system& ref) : middleman(ref), backend_(&ref) {
        // nop
      }

      network::multiplexer& backend() override {
        return backend_;
      }

    private:
      Backend backend_;
    };
    return new impl(sys);
  }

  /// Returns the heartbeat interval in milliseconds.
  inline size_t heartbeat_interval() const {
    return heartbeat_interval_;
  }

  /// Retruns whether the middleman tries to establish
  /// a direct connection to each of its peers.
  inline bool enable_automatic_connections() const {
    return enable_automatic_connections_;
  }

protected:
  middleman(actor_system& ref);

private:
  template <spawn_options Os, class Impl, class F, class... Ts>
  typename infer_handle_from_class<Impl>::type
  spawn_client_impl(F fun, const std::string& host, uint16_t port, Ts&&... xs) {
    auto hdl = backend().new_tcp_scribe(host, port);
    detail::init_fun_factory<Impl, F> fac;
    actor_config cfg{&backend()};
    auto init = fac(std::move(fun), hdl, std::forward<Ts>(xs)...);
    cfg.init_fun = [hdl, init](local_actor* ptr) -> behavior {
      static_cast<abstract_broker*>(ptr)->assign_tcp_scribe(hdl);
      return init(ptr);
    };
    return system().spawn_class<Impl, Os>(cfg);
  }

  template <spawn_options Os, class Impl, class F, class... Ts>
  typename infer_handle_from_class<Impl>::type
  spawn_server_impl(F fun, uint16_t port, Ts&&... xs) {
    detail::init_fun_factory<Impl, F> fac;
    auto init = fac(std::move(fun), std::forward<Ts>(xs)...);
    auto hdl = backend().new_tcp_doorman(port).first;
    actor_config cfg{&backend()};
    cfg.init_fun = [hdl, init](local_actor* ptr) -> behavior {
      static_cast<abstract_broker*>(ptr)->assign_tcp_doorman(hdl);
      return init(ptr);
    };
    return system().spawn_class<Impl, Os>(cfg);
  }

  uint16_t publish(const strong_actor_ptr& whom, std::set<std::string> sigs,
                   uint16_t port, const char* in, bool ru);

  void unpublish(const actor_addr& whom, uint16_t port);

  strong_actor_ptr remote_actor(std::set<std::string> ifs,
                                std::string host, uint16_t port);

  // environment
  actor_system& system_;
  // prevents backend from shutting down unless explicitly requested
  network::multiplexer::supervisor_ptr backend_supervisor_;
  // runs the backend
  std::thread thread_;
  // keeps track of "singleton-like" brokers
  std::map<atom_value, actor> named_brokers_;
  // user-defined hooks
  hook_uptr hooks_;
  // actor offering asyncronous IO by managing this singleton instance
  middleman_actor manager_;
  // heartbeat interval of BASP in milliseconds
  size_t heartbeat_interval_;
  // configures whether BASP tries to connect to all known peers
  bool enable_automatic_connections_;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_MIDDLEMAN_HPP
