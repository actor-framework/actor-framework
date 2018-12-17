/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <map>
#include <vector>
#include <memory>
#include <thread>

#include "caf/actor_system.hpp"
#include "caf/detail/unique_function.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/send.hpp"

#include "caf/io/hook.hpp"
#include "caf/io/broker.hpp"
#include "caf/io/middleman_actor.hpp"
#include "caf/io/network/multiplexer.hpp"

namespace caf {
namespace io {

/// Manages brokers and network backends.
class middleman : public actor_system::module {
public:
  friend class ::caf::actor_system;

  using hook_vector = std::vector<hook_uptr>;

  ~middleman() override;

  /// Tries to open a port for other CAF instances to connect to.
  /// @experimental
  expected<uint16_t> open(uint16_t port, const char* in = nullptr,
                          bool reuse = false);

  /// Closes port `port` regardless of whether an actor is published to it.
  expected<void> close(uint16_t port);

  /// Tries to connect to given node.
  /// @experimental
  expected<node_id> connect(std::string host, uint16_t port);

  /// Tries to publish `whom` at `port` and returns either an
  /// `error` or the bound port.
  /// @param whom Actor that should be published at `port`.
  /// @param port Unused TCP port.
  /// @param in The IP address to listen to or `INADDR_ANY` if `in == nullptr`.
  /// @param reuse Create socket using `SO_REUSEADDR`.
  /// @returns The actual port the OS uses after `bind()`. If `port == 0`
  ///          the OS chooses a random high-level port.
  template <class Handle>
  expected<uint16_t> publish(Handle&& whom, uint16_t port,
                             const char* in = nullptr, bool reuse = false) {
    detail::type_list<typename std::decay<Handle>::type> tk;
    return publish(actor_cast<strong_actor_ptr>(std::forward<Handle>(whom)),
                   system().message_types(tk), port, in, reuse);
  }

  /// Tries to publish `whom` at `port` and returns either an
  /// `error` or the bound port.
  /// @param whom Actor that should be published at `port`.
  /// @param port Unused UDP port.
  /// @param in The IP address to listen to or `INADDR_ANY` if `in == nullptr`.
  /// @param reuse Create socket using `SO_REUSEADDR`.
  /// @returns The actual port the OS uses after `bind()`. If `port == 0`
  ///          the OS chooses a random high-level port.
  template <class Handle>
  expected<uint16_t> publish_udp(Handle&& whom, uint16_t port,
                                 const char* in = nullptr, bool reuse = false) {
    detail::type_list<typename std::decay<Handle>::type> tk;
    return publish_udp(actor_cast<strong_actor_ptr>(std::forward<Handle>(whom)),
                       system().message_types(tk), port, in, reuse);
  }

  /// Makes *all* local groups accessible via network
  /// on address `addr` and `port`.
  /// @returns The actual port the OS uses after `bind()`. If `port == 0`
  ///          the OS chooses a random high-level port.
  expected<uint16_t> publish_local_groups(uint16_t port,
                                          const char* in = nullptr,
                                          bool reuse = false);

  /// Unpublishes `whom` by closing `port` or all assigned ports if `port == 0`.
  /// @param whom Actor that should be unpublished at `port`.
  /// @param port TCP port.
  template <class Handle>
  expected<void> unpublish(const Handle& whom, uint16_t port = 0) {
    return unpublish(whom.address(), port);
  }

  /// Unpublishes `whom` by closing `port` or all assigned ports if `port == 0`.
  /// @param whom Actor that should be unpublished at `port`.
  /// @param port UDP port.
  template <class Handle>
  expected<void> unpublish_udp(const Handle& whom, uint16_t port = 0) {
    return unpublish_udp(whom.address(), port);
  }

  /// Establish a new connection to the actor at `host` on given `port`.
  /// @param host Valid hostname or IP address.
  /// @param port TCP port.
  /// @returns An `actor` to the proxy instance representing
  ///          a remote actor or an `error`.
  template <class ActorHandle = actor>
  expected<ActorHandle> remote_actor(std::string host, uint16_t port) {
    detail::type_list<ActorHandle> tk;
    auto x = remote_actor(system().message_types(tk), std::move(host), port);
    if (!x)
      return x.error();
    CAF_ASSERT(x && *x);
    return actor_cast<ActorHandle>(std::move(*x));
  }

  /// Contacts the actor at `host` on given `port`.
  /// @param host Valid hostname or IP address.
  /// @param port UDP port.
  /// @returns An `actor` to the proxy instance representing
  ///          a remote actor or an `error`.
  template <class ActorHandle = actor>
  expected<ActorHandle> remote_actor_udp(std::string host, uint16_t port) {
    detail::type_list<ActorHandle> tk;
    auto x = remote_actor_udp(system().message_types(tk), std::move(host), port);
    if (!x)
      return x.error();
    CAF_ASSERT(x && *x);
    return actor_cast<ActorHandle>(std::move(*x));
  }

  /// <group-name>@<host>:<port>
  expected<group> remote_group(const std::string& group_uri);

  expected<group> remote_group(const std::string& group_identifier,
                               const std::string& host, uint16_t port);

  /// Returns the enclosing actor system.
  inline actor_system& system() {
    return system_;
  }

  /// Returns the systemw-wide configuration.
  inline const actor_system_config& config() const {
    return system_.config();
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
    for (auto& hook : hooks_)
      hook->handle<Event>(std::forward<Ts>(ts)...);
  }

  /// Returns whether this middleman has any hooks installed.
  inline bool has_hook() const {
    return !hooks_.empty();
  }

  /// Returns all installed hooks.
  const hook_vector& hooks() const {
    return hooks_;
  }

  /// Returns the actor associated with `name` at `nid` or
  /// `invalid_actor` if `nid` is not connected or has no actor
  /// associated to this `name`.
  /// @note Blocks the caller until `nid` responded to the lookup
  ///       or an error occurred.
  strong_actor_ptr remote_lookup(atom_value name, const node_id& nid);

  /// @experimental
  template <class Handle>
  expected<Handle>
  remote_spawn(const node_id& nid, std::string name, message args,
               duration timeout = duration(time_unit::minutes, 1)) {
    if (!nid || name.empty())
      return sec::invalid_argument;
    auto res = remote_spawn_impl(nid, name, args,
                                 system().message_types<Handle>(), timeout);
    if (!res)
      return std::move(res.error());
    return actor_cast<Handle>(std::move(*res));
  }

  /// @experimental
  template <class Handle, class Rep, class Period>
  expected<Handle> remote_spawn(const node_id& nid, std::string name,
                                message args,
                                std::chrono::duration<Rep, Period> timeout) {
    return remote_spawn<Handle>(nid, std::move(name), std::move(args),
                                duration{timeout});
  }

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
    using impl = infer_impl_from_fun_t<F>;
    static constexpr bool spawnable = detail::spawnable<F, impl, Ts...>();
    static_assert(spawnable,
                  "cannot spawn function-based broker with given arguments");
    actor_config cfg{&backend()};
    detail::bool_token<spawnable> enabled;
    return system().spawn_functor<Os>(enabled, cfg, fun,
                                      std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based broker connected
  /// to `host:port` or an `error`.
  /// @warning Blocks the caller for the duration of the connection process.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  expected<typename infer_handle_from_fun<F>::type>
  spawn_client(F fun, const std::string& host, uint16_t port, Ts&&... xs) {
    using impl = typename infer_handle_from_fun<F>::impl;
    return spawn_client_impl<Os, impl>(std::move(fun), host, port,
                                       std::forward<Ts>(xs)...);
  }

  /// Spawns a new broker as server running on given `port`.
  /// @warning Blocks the caller until the server socket is initialized.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  expected<typename infer_handle_from_fun<F>::type>
  spawn_server(F fun, uint16_t& port, Ts&&... xs) {
    using impl = typename infer_handle_from_fun<F>::impl;
    return spawn_server_impl<Os, impl>(std::move(fun), port,
                                       std::forward<Ts>(xs)...);
  }

  /// Spawns a new broker as server running on given `port`.
  /// @warning Blocks the caller until the server socket is initialized.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  expected<typename infer_handle_from_fun<F>::type>
  spawn_server(F fun, const uint16_t& port, Ts&&... xs) {
    uint16_t dummy = port;
    using impl = typename infer_handle_from_fun<F>::impl;
    return spawn_server_impl<Os, impl>(std::move(fun), dummy,
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

protected:
  middleman(actor_system& sys);

private:
  template <spawn_options Os, class Impl, class F, class... Ts>
  expected<typename infer_handle_from_class<Impl>::type>
  spawn_client_impl(F fun, const std::string& host, uint16_t port, Ts&&... xs) {
    auto eptr = backend().new_tcp_scribe(host, port);
    if (!eptr)
      return eptr.error();
    auto ptr = std::move(*eptr);
    CAF_ASSERT(ptr != nullptr);
    detail::init_fun_factory<Impl, F> fac;
    actor_config cfg{&backend()};
    auto fptr = fac.make(std::move(fun), ptr->hdl(), std::forward<Ts>(xs)...);
    fptr->hook([=](local_actor* self) mutable {
      static_cast<abstract_broker*>(self)->add_scribe(std::move(ptr));
    });
    cfg.init_fun.assign(fptr.release());
    return system().spawn_class<Impl, Os>(cfg);
  }

  template <spawn_options Os, class Impl, class F, class... Ts>
  expected<typename infer_handle_from_class<Impl>::type>
  spawn_server_impl(F fun, uint16_t& port, Ts&&... xs) {
    auto eptr = backend().new_tcp_doorman(port);
    if (!eptr)
      return eptr.error();
    auto ptr = std::move(*eptr);
    detail::init_fun_factory<Impl, F> fac;
    port = ptr->port();
    auto fptr = fac.make(std::move(fun), std::forward<Ts>(xs)...);
    fptr->hook([=](local_actor* self) mutable {
      static_cast<abstract_broker*>(self)->add_doorman(std::move(ptr));
    });
    actor_config cfg{&backend()};
    cfg.init_fun.assign(fptr.release());
    return system().spawn_class<Impl, Os>(cfg);
  }

  expected<strong_actor_ptr> remote_spawn_impl(const node_id& nid,
                                               std::string& name, message& args,
                                               std::set<std::string> s,
                                               duration timeout);

  expected<uint16_t> publish(const strong_actor_ptr& whom,
                             std::set<std::string> sigs,
                             uint16_t port, const char* cstr, bool ru);

  expected<uint16_t> publish_udp(const strong_actor_ptr& whom,
                                 std::set<std::string> sigs,
                                 uint16_t port, const char* cstr, bool ru);

  expected<void> unpublish(const actor_addr& whom, uint16_t port);

  expected<void> unpublish_udp(const actor_addr& whom, uint16_t port);

  expected<strong_actor_ptr> remote_actor(std::set<std::string> ifs,
                                          std::string host, uint16_t port);

  expected<strong_actor_ptr> remote_actor_udp(std::set<std::string> ifs,
                                              std::string host, uint16_t port);

  static int exec_slave_mode(actor_system&, const actor_system_config&);

  // environment
  actor_system& system_;
  // prevents backend from shutting down unless explicitly requested
  network::multiplexer::supervisor_ptr backend_supervisor_;
  // runs the backend
  std::thread thread_;
  // keeps track of "singleton-like" brokers
  std::map<atom_value, actor> named_brokers_;
  // user-defined hooks
  hook_vector hooks_;
  // actor offering asyncronous IO by managing this singleton instance
  middleman_actor manager_;
};

} // namespace io
} // namespace caf

