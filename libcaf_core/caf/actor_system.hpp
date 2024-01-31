// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_clock.hpp"
#include "caf/actor_config.hpp"
#include "caf/actor_profiler.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_traits.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/init_fun_factory.hpp"
#include "caf/detail/private_thread_pool.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/detail/spawn_fwd.hpp"
#include "caf/detail/spawnable.hpp"
#include "caf/fwd.hpp"
#include "caf/infer_handle.hpp"
#include "caf/is_typed_actor.hpp"
#include "caf/logger.hpp"
#include "caf/make_actor.hpp"
#include "caf/prohibit_top_level_spawn_marker.hpp"
#include "caf/scoped_execution_unit.hpp"
#include "caf/spawn_options.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/telemetry/metric_registry.hpp"
#include "caf/type_id.hpp"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <typeinfo>

namespace caf::detail {

template <class>
struct typed_mpi_access;

template <class Out, class... In>
struct typed_mpi_access<Out(In...)> {
  std::string operator()() const {
    static_assert(sizeof...(In) > 0, "typed MPI without inputs");
    std::vector<std::string> inputs{type_name_v<In>...};
    std::string result = "(";
    result += join(inputs, ",");
    result += ") -> ";
    result += type_name_v<Out>;
    return result;
  }
};

template <class... Out, class... In>
struct typed_mpi_access<result<Out...>(In...)> {
  std::string operator()() const {
    static_assert(sizeof...(In) > 0, "typed MPI without inputs");
    static_assert(sizeof...(Out) > 0, "typed MPI without outputs");
    std::vector<std::string> inputs{std::string{type_name_v<In>}...};
    std::vector<std::string> outputs1{std::string{type_name_v<Out>}...};
    std::string result = "(";
    result += join(inputs, ",");
    result += ") -> (";
    result += join(outputs1, ",");
    result += ")";
    return result;
  }
};

template <class T>
std::string get_rtti_from_mpi() {
  typed_mpi_access<T> f;
  return f();
}

} // namespace caf::detail

namespace caf {

/// Actor environment including scheduler, registry, and optional
/// components such as a middleman.
class CAF_CORE_EXPORT actor_system {
public:
  friend class logger;
  friend class io::middleman;
  friend class net::middleman;
  friend class abstract_actor;

  template <class>
  friend class actor_from_state_t;

  /// Returns the internal actor for dynamic spawn operations.
  const strong_actor_ptr& spawn_serv() const {
    return spawn_serv_;
  }

  /// Returns the internal actor for storing the runtime configuration
  /// for this actor system.
  const strong_actor_ptr& config_serv() const {
    return config_serv_;
  }

  actor_system() = delete;
  actor_system(const actor_system&) = delete;
  actor_system& operator=(const actor_system&) = delete;

  /// Calls a cleanup function in its destructor for cleaning up global state.
  class [[nodiscard]] global_state_guard {
  public:
    using void_fun_t = void (*)();

    explicit global_state_guard(void_fun_t f) : fun_(f) {
      // nop
    }

    global_state_guard(global_state_guard&& other) noexcept : fun_(other.fun_) {
      other.fun_ = nullptr;
    }

    global_state_guard& operator=(global_state_guard&& other) noexcept {
      std::swap(fun_, other.fun_);
      return *this;
    }

    global_state_guard() = delete;

    global_state_guard(const global_state_guard&) = delete;

    global_state_guard& operator=(const global_state_guard&) = delete;

    ~global_state_guard() {
      if (fun_ != nullptr)
        fun_();
    }

  private:
    void_fun_t fun_;
  };

  /// An (optional) component of the actor system.
  class CAF_CORE_EXPORT module {
  public:
    enum id_t {
      scheduler,
      middleman,
      openssl_manager,
      network_manager,
      num_ids
    };

    virtual ~module();

    /// Returns the human-readable name of the module.
    const char* name() const noexcept;

    /// Starts any background threads needed by the module.
    virtual void start() = 0;

    /// Stops all background threads of the module.
    virtual void stop() = 0;

    /// Allows the module to change the
    /// configuration of the actor system during startup.
    virtual void init(actor_system_config&) = 0;

    /// Returns the identifier of this module.
    virtual id_t id() const = 0;

    /// Returns a pointer to the subtype.
    virtual void* subtype_ptr() = 0;
  };

  using module_ptr = std::unique_ptr<module>;

  using module_array = std::array<module_ptr, module::num_ids>;

  /// An (optional) component of the actor system with networking
  /// capabilities.
  class CAF_CORE_EXPORT networking_module : public module {
  public:
    ~networking_module() override;

    /// Causes the module to send a `node_down_msg` to `observer` if this system
    /// loses connection to `node`.
    virtual void monitor(const node_id& node, const actor_addr& observer) = 0;

    /// Causes the module remove one entry for `observer` from the list of
    /// actors that receive a `node_down_msg` if this system loses connection to
    /// `node`. Each call to `monitor` requires one call to `demonitor` in order
    /// to unsubscribe the `observer` completely.
    virtual void demonitor(const node_id& node, const actor_addr& observer) = 0;
  };

  /// Metrics that the actor system collects by default.
  /// @warning Do not modify these metrics in user code. Some may be used by the
  ///          system for synchronization.
  struct base_metrics_t {
    /// Counts the number of messages that where rejected because the target
    /// mailbox was closed or did not exist.
    telemetry::int_counter* rejected_messages;

    /// Counts the total number of processed messages.
    telemetry::int_counter* processed_messages;

    /// Tracks the current number of running actors in the system.
    telemetry::int_gauge* running_actors;

    /// Counts the total number of messages that wait in a mailbox.
    telemetry::int_gauge* queued_messages;
  };

  /// Metrics that some actors may collect in addition to the base metrics. All
  /// families in this set use the label dimension *name* (the user-defined name
  /// of the actor).
  struct actor_metric_families_t {
    /// Samples how long the actor needs to process messages.
    telemetry::dbl_histogram_family* processing_time = nullptr;

    /// Samples how long a message waits in the mailbox before the actor
    /// processes it.
    telemetry::dbl_histogram_family* mailbox_time = nullptr;

    /// Counts how many messages are currently waiting in the mailbox.
    telemetry::int_gauge_family* mailbox_size = nullptr;

    struct {
      // -- inbound ------------------------------------------------------------

      /// Counts the total number of processed stream elements from upstream.
      telemetry::int_counter_family* processed_elements = nullptr;

      /// Tracks how many stream elements from upstream are currently buffered.
      telemetry::int_gauge_family* input_buffer_size = nullptr;

      // -- outbound -----------------------------------------------------------

      /// Counts the total number of elements that have been pushed downstream.
      telemetry::int_counter_family* pushed_elements = nullptr;

      /// Tracks how many stream elements are currently waiting in the output
      /// buffer due to insufficient credit.
      telemetry::int_gauge_family* output_buffer_size = nullptr;
    }

    /// Wraps streaming-related actor metric families.
    stream;
  };

  /// @warning The system stores a reference to `cfg`, which means the
  ///          config object must outlive the actor system.
  explicit actor_system(actor_system_config& cfg);

  virtual ~actor_system();

  /// A message passing interface (MPI) in run-time checkable representation.
  using mpi = std::set<std::string>;

  template <class T, class E = std::enable_if_t<!is_typed_actor_v<T>>>
  mpi message_types(detail::type_list<T>) const {
    return mpi{};
  }

  template <class... Ts>
  mpi message_types(detail::type_list<typed_actor<Ts...>>) const {
    static_assert(sizeof...(Ts) > 0, "empty typed actor handle given");
    mpi result{detail::get_rtti_from_mpi<Ts>()...};
    return result;
  }

  template <class T, class E = std::enable_if_t<!detail::is_type_list_v<T>>>
  mpi message_types(const T&) const {
    detail::type_list<T> token;
    return message_types(token);
  }

  /// Returns a string representation of the messaging
  /// interface using portable names;
  template <class T>
  mpi message_types() const {
    detail::type_list<T> token;
    return message_types(token);
  }

  /// Returns whether actor handles described by `xs`
  /// can be assigned to actor handles described by `ys`.
  /// @experimental
  bool assignable(const mpi& xs, const mpi& ys) const {
    if (ys.empty())
      return xs.empty();
    if (xs.size() == ys.size())
      return xs == ys;
    return std::includes(xs.begin(), xs.end(), ys.begin(), ys.end());
  }

  /// Returns whether actor handles described by `xs`
  /// can be assigned to actor handles of type `T`.
  /// @experimental
  template <class T>
  bool assignable(const std::set<std::string>& xs) const {
    return assignable(xs, message_types<T>());
  }

  /// Returns the metrics registry for this system.
  telemetry::metric_registry& metrics() noexcept {
    return metrics_;
  }

  /// Returns the metrics registry for this system.
  const telemetry::metric_registry& metrics() const noexcept {
    return metrics_;
  }

  /// Returns the host-local identifier for this system.
  const node_id& node() const {
    return node_;
  }

  /// Returns the scheduler instance.
  scheduler::abstract_coordinator& scheduler();

  /// Returns the system-wide event logger.
  caf::logger& logger();

  /// Returns the system-wide actor registry.
  actor_registry& registry();

  /// Returns `true` if the I/O module is available, `false` otherwise.
  bool has_middleman() const;

  /// Returns the middleman instance from the I/O module.
  /// @throws `std::logic_error` if module is not loaded.
  io::middleman& middleman();

  /// Returns `true` if the openssl module is available, `false` otherwise.
  bool has_openssl_manager() const;

  /// Returns the manager instance from the OpenSSL module.
  /// @throws `std::logic_error` if module is not loaded.
  openssl::manager& openssl_manager() const;

  /// Returns `true` if the network module is available, `false` otherwise.
  bool has_network_manager() const noexcept;

  /// Returns the network manager (middleman) instance.
  /// @throws `std::logic_error` if module is not loaded.
  net::middleman& network_manager();

  /// Returns a dummy execution unit that forwards
  /// everything to the scheduler.
  scoped_execution_unit* dummy_execution_unit();

  /// Returns a new actor ID.
  actor_id next_actor_id();

  /// Returns the last given actor ID.
  actor_id latest_actor_id() const;

  /// Blocks this caller until all actors are done.
  void await_all_actors_done() const;

  /// Send a `node_down_msg` to `observer` if this system loses connection to
  /// `node`.
  /// @note Calling this function *n* times causes the system to send
  ///       `node_down_msg` *n* times to the observer. In order to not receive
  ///       the messages, the observer must call `demonitor` *n* times.
  void monitor(const node_id& node, const actor_addr& observer);

  /// Removes `observer` from the list of actors that receive a `node_down_msg`
  /// if this system loses connection to `node`.
  void demonitor(const node_id& node, const actor_addr& observer);

  /// Called by `spawn` when used to create a class-based actor to
  /// apply automatic conversions to `xs` before spawning the actor.
  /// Should not be called by users of the library directly.
  /// @param cfg To-be-filled config for the actor.
  /// @param xs Constructor arguments for `C`.
  template <class C, spawn_options Os, class... Ts>
  infer_handle_from_class_t<C> spawn_class(actor_config& cfg, Ts&&... xs) {
    return spawn_impl<C, Os>(cfg, detail::spawn_fwd<Ts>(xs)...);
  }

  /// Returns a new actor of type `C` using `xs...` as constructor
  /// arguments. The behavior of `spawn` can be modified by setting `Os`, e.g.,
  /// to opt-out of the cooperative scheduling.
  /// @param xs Constructor arguments for `C`.
  template <class C, spawn_options Os = no_spawn_options, class... Ts>
  infer_handle_from_class_t<C> spawn(Ts&&... xs) {
    check_invariants<C>();
    actor_config cfg;
    cfg.mbox_factory = mailbox_factory();
    return spawn_impl<C, Os>(cfg, detail::spawn_fwd<Ts>(xs)...);
  }

  /// Called by `spawn` when used to create a functor-based actor to select a
  /// proper implementation and then delegates to `spawn_impl`.
  /// @param cfg To-be-filled config for the actor.
  /// @param fun Function object for the actor's behavior; will be moved.
  /// @param xs Arguments for `fun`.
  /// @private
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  infer_handle_from_fun_t<F>
  spawn_functor(std::true_type, actor_config& cfg, F& fun, Ts&&... xs) {
    using impl = infer_impl_from_fun_t<F>;
    detail::init_fun_factory<impl, F> fac;
    cfg.init_fun = fac(std::move(fun), std::forward<Ts>(xs)...);
    return spawn_impl<impl, Os>(cfg);
  }

  /// Fallback no-op overload.
  /// @private
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  infer_handle_from_fun_t<F>
  spawn_functor(std::false_type, actor_config&, F&, Ts&&...) {
    return {};
  }

  /// Returns a new functor-based actor. The first argument must be the functor,
  /// the remainder of `xs...` is used to invoke the functor.
  /// The behavior of `spawn` can be modified by setting `Os`, e.g.,
  /// to opt-out of the cooperative scheduling.
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  infer_handle_from_fun_t<F> spawn(F fun, Ts&&... xs) {
    using impl = infer_impl_from_fun_t<F>;
    check_invariants<impl>();
    static constexpr bool spawnable = detail::spawnable<F, impl, Ts...>();
    static_assert(spawnable,
                  "cannot spawn function-based actor with given arguments");
    actor_config cfg;
    cfg.mbox_factory = mailbox_factory();
    return spawn_functor<Os>(std::bool_constant<spawnable>{}, cfg, fun,
                             std::forward<Ts>(xs)...);
  }

  /// Returns a new stateful actor.
  template <spawn_options Options = no_spawn_options, class CustomSpawn,
            class... Args>
  typename CustomSpawn::handle_type spawn(CustomSpawn, Args&&... args) {
    return CustomSpawn::template do_spawn<Options>(*this,
                                                   std::forward<Args>(args)...);
  }

  /// Returns a new actor with run-time type `name`, constructed
  /// with the arguments stored in `args`.
  /// @experimental
  template <class Handle, class E = std::enable_if_t<is_handle_v<Handle>>>
  expected<Handle>
  spawn(const std::string& name, message args, execution_unit* ctx = nullptr,
        bool check_interface = true, const mpi* expected_ifs = nullptr) {
    mpi tmp;
    if (check_interface && !expected_ifs) {
      tmp = message_types<Handle>();
      expected_ifs = &tmp;
    }
    auto res = dyn_spawn_impl(name, args, ctx, check_interface, expected_ifs);
    if (!res)
      return std::move(res.error());
    return actor_cast<Handle>(std::move(*res));
  }

  /// Returns whether this actor system calls `await_all_actors_done`
  /// in its destructor before shutting down.
  bool await_actors_before_shutdown() const {
    return await_actors_before_shutdown_;
  }

  /// Configures whether this actor system calls `await_all_actors_done`
  /// in its destructor before shutting down.
  void await_actors_before_shutdown(bool x) {
    await_actors_before_shutdown_ = x;
  }

  /// Returns the configuration of this actor system.
  const actor_system_config& config() const {
    return cfg_;
  }

  /// Returns the system-wide clock.
  actor_clock& clock() noexcept;

  /// Returns the number of detached actors.
  size_t detached_actors() const noexcept;

  /// @cond PRIVATE

  /// Calls all thread started hooks
  /// @warning must be called by thread which is about to start
  void thread_started(thread_owner);

  /// Calls all thread terminates hooks
  /// @warning must be called by thread which is about to terminate
  void thread_terminates();

  template <class F>
  std::thread launch_thread(const char* thread_name, thread_owner tag, F fun) {
    auto body = [this, thread_name, tag, f{std::move(fun)}](auto guard) {
      CAF_IGNORE_UNUSED(guard);
      CAF_SET_LOGGER_SYS(this);
      detail::set_thread_name(thread_name);
      thread_started(tag);
      f();
      thread_terminates();
    };
    return std::thread{std::move(body), meta_objects_guard_};
  }

  auto meta_objects_guard() const noexcept {
    return meta_objects_guard_;
  }

  const auto& metrics_actors_includes() const noexcept {
    return metrics_actors_includes_;
  }

  const auto& metrics_actors_excludes() const noexcept {
    return metrics_actors_excludes_;
  }

  template <class C, spawn_options Os, class... Ts>
  infer_handle_from_class_t<C> spawn_impl(actor_config& cfg, Ts&&... xs) {
    static_assert(is_unbound(Os),
                  "top-level spawns cannot have monitor or link flag");
    if constexpr (has_detach_flag(Os) || std::is_base_of_v<blocking_actor, C>)
      cfg.flags |= abstract_actor::is_detached_flag;
    if constexpr (has_hide_flag(Os))
      cfg.flags |= abstract_actor::is_hidden_flag;
    if (cfg.host == nullptr)
      cfg.host = dummy_execution_unit();
    CAF_SET_LOGGER_SYS(this);
    auto res = make_actor<C>(next_actor_id(), node(), this, cfg,
                             std::forward<Ts>(xs)...);
    auto ptr = static_cast<C*>(actor_cast<abstract_actor*>(res));
#ifdef CAF_ENABLE_ACTOR_PROFILER
    profiler_add_actor(*ptr, cfg.parent);
#endif
    ptr->launch(cfg.host, has_lazy_init_flag(Os), has_hide_flag(Os));
    return res;
  }

  /// Utility function object that allows users to explicitly launch an action
  /// by calling `operator()` but calls it implicitly at scope exit.
  template <class F>
  class launcher {
  public:
    launcher() : ready(false) {
      // nop
    }

    explicit launcher(F&& f) : ready(true) {
      new (&fn) F(std::move(f));
    }

    launcher(launcher&& other) : ready(other.ready) {
      if (ready) {
        new (&fn) F(std::move(other.fn));
        other.reset();
      }
    }

    launcher& operator=(launcher&& other) {
      if (this != &other) {
        if (ready)
          reset();
        if (other.ready) {
          ready = true;
          new (&fn) F(std::move(other.fn));
          other.reset();
        }
      }
      return *this;
    }

    launcher(const launcher&) = delete;

    launcher& operator=(const launcher&) = delete;

    ~launcher() {
      if (ready) {
        fn();
        fn.~F();
      }
    }

    void operator()() {
      if (ready) {
        fn();
        reset();
      }
    }

  private:
    // @pre `ready == true`
    void reset() {
      CAF_ASSERT(ready);
      ready = false;
      fn.~F();
    }

    bool ready;
    union {
      F fn;
    };
  };

  /// Creates a new, cooperatively scheduled actor. The returned actor is
  /// constructed but has not been added to the scheduler yet to allow the
  /// caller to set up any additional logic on the actor before it starts.
  /// @returns A pointer to the new actor and a function object that the caller
  ///          must invoke to launch the actor. After the actor started running,
  ///          the caller *must not* access the pointer again.
  template <class Impl, spawn_options Os = no_spawn_options, class... Ts>
  auto spawn_inactive(Ts&&... xs) {
    static_assert(std::is_base_of_v<scheduled_actor, Impl>,
                  "only scheduled actors may get spawned inactively");
    CAF_SET_LOGGER_SYS(this);
    actor_config cfg{dummy_execution_unit(), nullptr};
    if constexpr (has_detach_flag(Os))
      cfg.flags |= abstract_actor::is_detached_flag;
    if constexpr (has_hide_flag(Os))
      cfg.flags |= abstract_actor::is_hidden_flag;
    cfg.mbox_factory = mailbox_factory();
    auto res = make_actor<Impl>(next_actor_id(), node(), this, cfg,
                                std::forward<Ts>(xs)...);
    auto ptr = static_cast<Impl*>(actor_cast<abstract_actor*>(res));
#ifdef CAF_ENABLE_ACTOR_PROFILER
    profiler_add_actor(*ptr, cfg.parent);
#endif
    auto launch = [strong_ptr = std::move(res), host{cfg.host}] {
      // Note: we pass `res` to this lambda instead of `ptr` to keep a strong
      //       reference to the actor.
      static_cast<Impl*>(actor_cast<abstract_actor*>(strong_ptr))
        ->launch(host, has_lazy_init_flag(Os), has_hide_flag(Os));
    };
    return std::make_tuple(ptr, launcher<decltype(launch)>(std::move(launch)));
  }

  void profiler_add_actor(const local_actor& self, const local_actor* parent) {
    if (profiler_)
      profiler_->add_actor(self, parent);
  }

  void profiler_remove_actor(const local_actor& self) {
    if (profiler_)
      profiler_->remove_actor(self);
  }

  void profiler_before_processing(const local_actor& self,
                                  const mailbox_element& element) {
    if (profiler_)
      profiler_->before_processing(self, element);
  }

  void profiler_after_processing(const local_actor& self,
                                 invoke_message_result result) {
    if (profiler_)
      profiler_->after_processing(self, result);
  }

  void profiler_before_sending(const local_actor& self,
                               mailbox_element& element) {
    if (profiler_)
      profiler_->before_sending(self, element);
  }

  void profiler_before_sending_scheduled(const local_actor& self,
                                         caf::actor_clock::time_point timeout,
                                         mailbox_element& element) {
    if (profiler_)
      profiler_->before_sending_scheduled(self, timeout, element);
  }

  base_metrics_t& base_metrics() noexcept {
    return base_metrics_;
  }

  const auto& base_metrics() const noexcept {
    return base_metrics_;
  }

  const auto& actor_metric_families() const noexcept {
    return actor_metric_families_;
  }

  tracing_data_factory* tracing_context() const noexcept {
    return tracing_context_;
  }

  detail::private_thread* acquire_private_thread();

  void release_private_thread(detail::private_thread*);

  /// @endcond

private:
  template <class T>
  void check_invariants() {
    static_assert(!std::is_base_of_v<prohibit_top_level_spawn_marker, T>,
                  "This actor type cannot be spawned through an actor system. "
                  "Probably you have tried to spawn a broker.");
  }

  expected<strong_actor_ptr>
  dyn_spawn_impl(const std::string& name, message& args, execution_unit* ctx,
                 bool check_interface, const mpi* expected_ifs);

  /// Sets the internal actor for dynamic spawn operations.
  void spawn_serv(strong_actor_ptr x) {
    spawn_serv_ = std::move(x);
  }

  /// Sets the internal actor for storing the runtime configuration.
  void config_serv(strong_actor_ptr x) {
    config_serv_ = std::move(x);
  }

  detail::mailbox_factory* mailbox_factory();

  // -- member variables -------------------------------------------------------

  /// Provides system-wide callbacks for several actor operations.
  actor_profiler* profiler_;

  /// Used to generate ascending actor IDs.
  std::atomic<size_t> ids_;

  /// Manages all metrics collected by the system.
  telemetry::metric_registry metrics_;

  /// Stores all metrics that the actor system collects by default.
  base_metrics_t base_metrics_;

  /// Identifies this actor system in a distributed setting.
  node_id node_;

  /// Manages log output.
  intrusive_ptr<caf::logger> logger_;

  /// Maps well-known actor names to actor handles.
  actor_registry registry_;

  /// Stores optional actor system components.
  module_array modules_;

  /// Provides pseudo scheduling context to actors.
  scoped_execution_unit dummy_execution_unit_;

  /// Stores whether the system should wait for running actors on shutdown.
  bool await_actors_before_shutdown_;

  /// Stores config parameters.
  strong_actor_ptr config_serv_;

  /// Allows fully dynamic spawning of actors.
  strong_actor_ptr spawn_serv_;

  /// The system-wide, user-provided configuration.
  actor_system_config& cfg_;

  /// Stores the system-wide factory for deserializing tracing data.
  tracing_data_factory* tracing_context_;

  /// Caches the configuration parameter `caf.metrics-filters.actors.includes`
  /// for faster lookups at runtime.
  std::vector<std::string> metrics_actors_includes_;

  /// Caches the configuration parameter `caf.metrics-filters.actors.excludes`
  /// for faster lookups at runtime.
  std::vector<std::string> metrics_actors_excludes_;

  /// Caches families for optional actor metrics.
  actor_metric_families_t actor_metric_families_;

  /// Manages threads for detached actors.
  detail::private_thread_pool private_threads_;

  /// Ties the lifetime of the meta objects table to the actor system.
  detail::global_meta_objects_guard_type meta_objects_guard_;
};

} // namespace caf
