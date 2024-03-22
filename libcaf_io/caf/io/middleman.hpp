// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/broker.hpp"
#include "caf/io/middleman_actor.hpp"
#include "caf/io/network/multiplexer.hpp"

#include "caf/actor_system.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/detail/unique_function.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/infer_handle.hpp"
#include "caf/node_id.hpp"
#include "caf/send.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace caf::io {

/// Manages brokers and network backends.
class CAF_IO_EXPORT middleman : public actor_system::networking_module {
public:
  friend class actor_system;

  /// Metrics that the middleman collects by default.
  struct metric_singletons_t {
    /// Samples the size of inbound messages before deserializing them.
    telemetry::int_histogram* inbound_messages_size = nullptr;

    /// Samples how long the middleman needs to deserialize inbound messages.
    telemetry::dbl_histogram* deserialization_time = nullptr;

    /// Samples the size of outbound messages after serializing them.
    telemetry::int_histogram* outbound_messages_size = nullptr;

    /// Samples how long the middleman needs to serialize outbound messages.
    telemetry::dbl_histogram* serialization_time = nullptr;
  };

  /// Independent tasks that run in the background, usually in their own thread.
  struct background_task {
    virtual ~background_task();
  };

  using background_task_ptr = std::unique_ptr<background_task>;

  /// Adds message types of the I/O module to the global meta object table.
  static void init_global_meta_objects();

  ~middleman() override;

  /// Tries to open a port for other CAF instances to connect to.
  expected<uint16_t> open(uint16_t port, const char* in = nullptr,
                          bool reuse = false);

  /// Closes port `port` regardless of whether an actor is published to it.
  expected<void> close(uint16_t port);

  /// Tries to connect to given node.
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
    type_list<std::decay_t<Handle>> tk;
    return publish(actor_cast<strong_actor_ptr>(std::forward<Handle>(whom)),
                   system().message_types(tk), port, in, reuse);
  }

  /// Unpublishes `whom` by closing `port` or all assigned ports if `port == 0`.
  /// @param whom Actor that should be unpublished at `port`.
  /// @param port TCP port.
  template <class Handle>
  expected<void> unpublish(const Handle& whom, uint16_t port = 0) {
    return unpublish(whom.address(), port);
  }

  /// Establish a new connection to the actor at `host` on given `port`.
  /// @param host Valid hostname or IP address.
  /// @param port TCP port.
  /// @returns An `actor` to the proxy instance representing
  ///          a remote actor or an `error`.
  template <class ActorHandle = actor>
  expected<ActorHandle> remote_actor(std::string host, uint16_t port) {
    type_list<ActorHandle> tk;
    auto x = remote_actor(system().message_types(tk), std::move(host), port);
    if (!x)
      return x.error();
    CAF_ASSERT(x && *x);
    return actor_cast<ActorHandle>(std::move(*x));
  }

  /// Returns the enclosing actor system.
  actor_system& system() {
    return system_;
  }

  /// Returns the systemw-wide configuration.
  const actor_system_config& config() const {
    return system_.config();
  }

  /// Returns a handle to the actor managing the middleman singleton.
  middleman_actor actor_handle();

  /// Returns the broker associated with `name` or creates a
  /// new instance of type `Impl`.
  template <class Impl>
  actor named_broker(const std::string& name) {
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

  /// Returns the actor associated with `name` at `nid` or
  /// `invalid_actor` if `nid` is not connected or has no actor
  /// associated to this `name`.
  /// @note Blocks the caller until `nid` responded to the lookup
  ///       or an error occurred.
  strong_actor_ptr remote_lookup(std::string name, const node_id& nid);

  template <class Handle>
  expected<Handle>
  remote_spawn(const node_id& nid, std::string name, message args,
               timespan timeout = timespan{std::chrono::minutes{1}}) {
    if (!nid || name.empty())
      return sec::invalid_argument;
    if (nid == system().node())
      return system().spawn<Handle>(std::move(name), std::move(args));
    auto res = remote_spawn_impl(nid, name, args,
                                 system().message_types<Handle>(), timeout);
    if (!res)
      return std::move(res.error());
    return actor_cast<Handle>(std::move(*res));
  }

  template <class Handle, class Rep, class Period>
  expected<Handle> remote_spawn(const node_id& nid, std::string name,
                                message args,
                                std::chrono::duration<Rep, Period> timeout) {
    return remote_spawn<Handle>(nid, std::move(name), std::move(args),
                                timespan{timeout});
  }

  /// Smart pointer for `network::multiplexer`.
  using backend_pointer = std::unique_ptr<network::multiplexer>;

  /// Used to initialize the backend during construction.
  using backend_factory = std::function<backend_pointer()>;

  void start() override;

  void stop() override;

  void init(actor_system_config&) override;

  id_t id() const override;

  void* subtype_ptr() override;

  void monitor(const node_id& node, const actor_addr& observer) override;

  void demonitor(const node_id& node, const actor_addr& observer) override;

  /// Spawns a new functor-based broker.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  infer_handle_from_fun_t<F> spawn_broker(F fun, Ts&&... xs) {
    using impl = infer_impl_from_fun_t<F>;
    static constexpr bool spawnable = detail::spawnable<F, impl, Ts...>();
    static_assert(spawnable,
                  "cannot spawn function-based broker with given arguments");
    actor_config cfg{&backend()};
    return system().spawn_functor<Os>(std::bool_constant<spawnable>{}, cfg, fun,
                                      std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based broker connected
  /// to `host:port` or an `error`.
  /// @warning Blocks the caller for the timespan of the connection process.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  expected<infer_handle_from_fun_t<F>>
  spawn_client(F fun, const std::string& host, uint16_t port, Ts&&... xs) {
    using impl = typename infer_handle_from_fun_trait_t<F>::impl;
    return spawn_client_impl<Os, impl>(std::move(fun), host, port,
                                       std::forward<Ts>(xs)...);
  }

  /// Spawns a new broker as server running on given `port`.
  /// @warning Blocks the caller until the server socket is initialized.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  expected<infer_handle_from_fun_t<F>>
  spawn_server(F fun, uint16_t& port, Ts&&... xs) {
    using impl = typename infer_handle_from_fun_trait_t<F>::impl;
    return spawn_server_impl<Os, impl>(std::move(fun), port,
                                       std::forward<Ts>(xs)...);
  }

  /// Spawns a new broker as server running on given `port`.
  /// @warning Blocks the caller until the server socket is initialized.
  template <spawn_options Os = no_spawn_options,
            class F = std::function<void(broker*)>, class... Ts>
  expected<infer_handle_from_fun_t<F>>
  spawn_server(F fun, const uint16_t& port, Ts&&... xs) {
    uint16_t dummy = port;
    using impl = typename infer_handle_from_fun_trait_t<F>::impl;
    return spawn_server_impl<Os, impl>(std::move(fun), dummy,
                                       std::forward<Ts>(xs)...);
  }

  /// Adds module-specific options to the config before loading the module.
  static void add_module_options(actor_system_config& cfg);

  /// Returns a middleman using the default network backend.
  static actor_system::module* make(actor_system&);

  /// @private
  actor get_named_broker(const std::string& name) {
    if (auto i = named_brokers_.find(name); i != named_brokers_.end())
      return i->second;
    return {};
  }

  /// @private
  metric_singletons_t metric_singletons;

  /// @private
  uint16_t prometheus_scraping_port() const noexcept {
    return prometheus_scraping_port_;
  }

protected:
  middleman(actor_system& sys);

private:
  template <spawn_options Os, class Impl, class F, class... Ts>
  expected<infer_handle_from_class_t<Impl>>
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
  expected<infer_handle_from_class_t<Impl>>
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

  expected<strong_actor_ptr>
  remote_spawn_impl(const node_id& nid, std::string& name, message& args,
                    std::set<std::string> s, timespan timeout);

  expected<uint16_t> publish(const strong_actor_ptr& whom,
                             std::set<std::string> sigs, uint16_t port,
                             const char* cstr, bool ru);

  expected<void> unpublish(const actor_addr& whom, uint16_t port);

  expected<strong_actor_ptr> remote_actor(std::set<std::string> ifs,
                                          std::string host, uint16_t port);

  /// The actor environment.
  actor_system& system_;

  /// Prevents backend from shutting down unless explicitly requested.
  network::multiplexer::supervisor_ptr backend_supervisor_;

  /// Runs the backend.
  std::thread thread_;

  /// Keeps track of "singleton-like" brokers.
  std::map<std::string, actor> named_brokers_;

  /// Offers an asynchronous IO by managing this singleton instance.
  middleman_actor manager_;

  /// Handles to tasks that we spin up in start() and destroy in stop().
  std::vector<background_task_ptr> background_tasks_;

  /// Stores the port where the Prometheus scraper is listening at (0 if no
  /// scraper is running in the background).
  uint16_t prometheus_scraping_port_ = 0;
};

} // namespace caf::io
