/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_ACTOR_SYSTEM_HPP
#define CAF_ACTOR_SYSTEM_HPP

#include <array>
#include <mutex>
#include <atomic>
#include <string>
#include <memory>
#include <cstddef>
#include <functional>
#include <condition_variable>

#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/actor_cast.hpp"
#include "caf/make_actor.hpp"
#include "caf/infer_handle.hpp"
#include "caf/actor_config.hpp"
#include "caf/spawn_options.hpp"
#include "caf/group_manager.hpp"
#include "caf/is_typed_actor.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/actor_registry.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/named_actor_config.hpp"
#include "caf/scoped_execution_unit.hpp"
#include "caf/uniform_type_info_map.hpp"
#include "caf/composable_behavior_based_actor.hpp"
#include "caf/prohibit_top_level_spawn_marker.hpp"

#include "caf/detail/spawn_fwd.hpp"
#include "caf/detail/init_fun_factory.hpp"

namespace caf {

using rtti_pair = std::pair<uint16_t, const std::type_info*>;

using rtti_pair_vec = std::vector<rtti_pair>;

using rtti_pair_vec_triple = std::tuple<rtti_pair_vec,
                                        rtti_pair_vec,
                                        rtti_pair_vec>;

template <class T>
struct mpi_field_access {
  std::string operator()(const uniform_type_info_map& types) {
    auto nr = type_nr<T>::value;
    if (nr != 0)
      return *types.portable_name(nr, nullptr);
    auto ptr = types.portable_name(0, &typeid(T));
    if (ptr != nullptr)
      return *ptr;
    std::string result = "<invalid-type[typeid ";
    result += typeid(T).name();
    result += "]>";
    return result;
  }
};

template <atom_value X>
struct mpi_field_access<atom_constant<X>> {
  std::string operator()(const uniform_type_info_map&) {
    return to_string(X);
  }
};

template <>
struct mpi_field_access<void> {
  std::string operator()(const uniform_type_info_map&) {
    return "void";
  }
};
template <class T>
std::string get_mpi_field(const uniform_type_info_map& types) {
  mpi_field_access<T> f;
  return f(types);
}

template <class T>
struct typed_mpi_access;

template <class... Is, class... Ls>
struct typed_mpi_access<typed_mpi<detail::type_list<Is...>,
                                  output_tuple<Ls...>>> {
  std::string operator()(const uniform_type_info_map& types) const {
    static_assert(sizeof...(Is) > 0, "typed MPI without inputs");
    static_assert(sizeof...(Ls) > 0, "typed MPI without outputs");
    std::vector<std::string> inputs{get_mpi_field<Is>(types)...};
    std::vector<std::string> outputs1{get_mpi_field<Ls>(types)...};
    std::string result = "caf::replies_to<";
    result += join(inputs, ",");
    result += ">::with<";
    result += join(outputs1, ",");
    result += ">";
    return result;
  }
};

template <class T>
std::string get_rtti_from_mpi(const uniform_type_info_map& types) {
  typed_mpi_access<T> f;
  return f(types);
}

/// Actor environment including scheduler, registry, and optional components
/// such as a middleman.
class actor_system {
public:
  friend class logger;
  friend class io::middleman;
  friend class abstract_actor;

  /// The number of actors implictly spawned by the actor system on startup.
  static constexpr size_t num_internal_actors = 3;

  /// Returns the ID of an internal actor by its name.
  /// @pre x in {'SpawnServ', 'ConfigServ', 'StreamServ'}
  static constexpr size_t internal_actor_id(atom_value x) {
    return x == atom("SpawnServ") ? 0 : (x == atom("ConfigServ") ? 1 : 2);
  }

  /// Returns the internal actor for dynamic spawn operations.
  inline const strong_actor_ptr& spawn_serv() const {
    return internal_actors_[internal_actor_id(atom("SpawnServ"))];
  }

  /// Returns the internal actor for storing the runtime configuration
  /// for this actor system.
  inline const strong_actor_ptr& config_serv() const {
    return internal_actors_[internal_actor_id(atom("ConfigServ"))];
  }

  /// Returns the internal actor for managing streams that
  /// cross network boundaries.
  inline const strong_actor_ptr& stream_serv() const {
    return internal_actors_[internal_actor_id(atom("StreamServ"))];
  }

  actor_system() = delete;
  actor_system(const actor_system&) = delete;
  actor_system& operator=(const actor_system&) = delete;

  /// An (optional) component of the actor system.
  class module {
  public:
    enum id_t {
      scheduler,
      middleman,
      opencl_manager,
      openssl_manager,
      num_ids
    };

    virtual ~module();

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

  using named_actor_config_map = std::unordered_map<std::string,
                                                    named_actor_config>;

  /// @warning The system stores a reference to `cfg`, which means the
  ///          config object must outlive the actor system.
  explicit actor_system(actor_system_config& cfg);

  virtual ~actor_system();

  /// A message passing interface (MPI) in run-time checkable representation.
  using mpi = std::set<std::string>;

  template <class T,
            class E = typename std::enable_if<!is_typed_actor<T>::value>::type>
  mpi message_types(detail::type_list<T>) const {
    return mpi{};
  }

  template <class... Ts>
  mpi message_types(detail::type_list<typed_actor<Ts...>>) const {
    static_assert(sizeof...(Ts) > 0, "empty typed actor handle given");
    mpi result{get_rtti_from_mpi<Ts>(types())...};
    return result;
  }

  template <class T,
            class E =
              typename std::enable_if<!detail::is_type_list<T>::value>::type>
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
  inline bool assignable(const mpi& xs, const mpi& ys) const {
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

  /// Returns the host-local identifier for this system.
  const node_id& node() const;

  /// Returns the scheduler instance.
  scheduler::abstract_coordinator& scheduler();

  /// Returns the system-wide event logger.
  caf::logger& logger();

  /// Returns the system-wide actor registry.
  actor_registry& registry();

  /// Returns the system-wide factory for custom types and actors.
  const uniform_type_info_map& types() const;

  /// Returns a string representation for `err`.
  std::string render(const error& x) const;

  /// Returns the system-wide group manager.
  group_manager& groups();

  /// Returns `true` if the I/O module is available, `false` otherwise.
  bool has_middleman() const;

  /// Returns the middleman instance from the I/O module.
  /// @throws `std::logic_error` if module is not loaded.
  io::middleman& middleman();

  /// Returns `true` if the opencl module is available, `false` otherwise.
  bool has_opencl_manager() const;

  /// Returns the manager instance from the OpenCL module.
  /// @throws `std::logic_error` if module is not loaded.
  opencl::manager& opencl_manager() const;

  /// Returns `true` if the openssl module is available, `false` otherwise.
  bool has_openssl_manager() const;

  /// Returns the manager instance from the OpenSSL module.
  /// @throws `std::logic_error` if module is not loaded.
  openssl::manager& openssl_manager() const;

  /// Returns a dummy execution unit that forwards
  /// everything to the scheduler.
  scoped_execution_unit* dummy_execution_unit();

  /// Returns a new actor ID.
  actor_id next_actor_id();

  /// Returns the last given actor ID.
  actor_id latest_actor_id() const;

  /// Blocks this caller until all actors are done.
  void await_all_actors_done() const;

  /// Called by `spawn` when used to create a class-based actor to
  /// apply automatic conversions to `xs` before spawning the actor.
  /// Should not be called by users of the library directly.
  /// @param cfg To-be-filled config for the actor.
  /// @param xs Constructor arguments for `C`.
  template <class C, spawn_options Os, class... Ts>
  infer_handle_from_class_t<C>
  spawn_class(actor_config& cfg, Ts&&... xs) {
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
    return spawn_impl<C, Os>(cfg, detail::spawn_fwd<Ts>(xs)...);
  }

  template <class S, spawn_options Os = no_spawn_options>
  infer_handle_from_state_t<S> spawn() {
    return spawn<composable_behavior_based_actor<S>, Os>();
  }

  /// Called by `spawn_functor` to apply static assertions and
  /// store an initialization function in `cfg` before calling `spawn_class`.
  /// @param cfg To-be-filled config for the actor.
  /// @param fun Function object for the actor's behavior; will be moved.
  /// @param xs Arguments for `fun`.
  template <spawn_options Os, class C, class F, class... Ts>
  infer_handle_from_class_t<C>
  spawn_functor_impl(actor_config& cfg, F& fun, Ts&&... xs) {
    detail::init_fun_factory<C, F> fac;
    cfg.init_fun = fac(std::move(fun), std::forward<Ts>(xs)...);
    return spawn_impl<C, Os>(cfg);
  }

  /// Called by `spawn` when used to create a functor-based actor to select
  /// a proper implementation and then delegates to `spawn_functor_impl`.
  /// Should not be called by users of the library directly.
  /// @param cfg To-be-filled config for the actor.
  /// @param fun Function object for the actor's behavior; will be moved.
  /// @param xs Arguments for `fun`.
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  infer_handle_from_fun_t<F>
  spawn_functor(actor_config& cfg, F& fun, Ts&&... xs) {
    using impl = infer_impl_from_fun_t<F>;
    return spawn_functor_impl<Os, impl>(cfg, fun, std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based actor. The first argument must be the functor,
  /// the remainder of `xs...` is used to invoke the functor.
  /// The behavior of `spawn` can be modified by setting `Os`, e.g.,
  /// to opt-out of the cooperative scheduling.
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  infer_handle_from_fun_t<F>
  spawn(F fun, Ts&&... xs) {
    check_invariants<infer_impl_from_fun_t<F>>();
    actor_config cfg;
    return spawn_functor<Os>(cfg, fun, std::forward<Ts>(xs)...);
  }

  /// Returns a new actor with run-time type `name`, constructed
  /// with the arguments stored in `args`.
  /// @experimental
  template <class Handle,
            class E = typename std::enable_if<is_handle<Handle>::value>::type>
  expected<Handle> spawn(const std::string& name, message args,
                         execution_unit* ctx = nullptr,
                         bool check_interface = true,
                         const mpi* expected_ifs = nullptr) {
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

  /// Spawns a class-based actor `T` immediately joining the groups in
  /// range `[first, last)`.
  /// @private
  template <class T, spawn_options Os, class Iter, class... Ts>
  infer_handle_from_class_t<T>
  spawn_class_in_groups(actor_config& cfg, Iter first, Iter last, Ts&&... xs) {
    static_assert(std::is_same<infer_handle_from_class_t<T>, actor>::value,
                  "Only dynamically typed actors can be spawned in a group.");
    check_invariants<T>();
    auto irange = make_input_range(first, last);
    cfg.groups = &irange;
    return spawn_class<T, Os>(cfg, std::forward<Ts>(xs)...);
  }

  /// Spawns a class-based actor `T` immediately joining the groups in
  /// range `[first, last)`.
  /// @private
  template <spawn_options Os, class Iter, class F, class... Ts>
  infer_handle_from_fun_t<F>
  spawn_fun_in_groups(actor_config& cfg, Iter first, Iter second,
                      F& fun, Ts&&... xs) {
    static_assert(std::is_same<infer_handle_from_fun_t<F>, actor>::value,
                  "Only dynamically actors can be spawned in a group.");
    check_invariants<infer_impl_from_fun_t<F>>();
    auto irange = make_input_range(first, second);
    cfg.groups = &irange;
    return spawn_functor<Os>(cfg, fun, std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based actor subscribed to all groups in `gs`.
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  infer_handle_from_fun_t<F>
  spawn_in_groups(std::initializer_list<group> gs, F fun, Ts&&... xs) {
    actor_config cfg;
    return spawn_fun_in_groups<Os>(cfg, gs.begin(), gs.end(), fun,
                                   std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based actor subscribed to all groups in `gs`.
  template <spawn_options Os = no_spawn_options, class Gs, class F, class... Ts>
  infer_handle_from_fun_t<F>
  spawn_in_groups(const Gs& gs, F fun, Ts&&... xs) {
    actor_config cfg;
    return spawn_fun_in_groups<Os>(cfg, gs.begin(), gs.end(), fun,
                                   std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based actor subscribed to all groups in `gs`.
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  infer_handle_from_fun_t<F>
  spawn_in_group(const group& grp, F fun, Ts&&... xs) {
    return spawn_in_groups<Os>({grp}, std::move(fun),
                               std::forward<Ts>(xs)...);
  }

  /// Returns a new class-based actor subscribed to all groups in `gs`.
  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  infer_handle_from_class_t<T>
  spawn_in_groups(std::initializer_list<group> gs, Ts&&... xs) {
    actor_config cfg;
    return spawn_class_in_groups<T, Os>(cfg, gs.begin(), gs.end(),
                                        std::forward<Ts>(xs)...);
  }

  /// Returns a new class-based actor subscribed to all groups in `gs`.
  template <class T, spawn_options Os = no_spawn_options, class Gs, class... Ts>
  infer_handle_from_class_t<T>
  spawn_in_groups(const Gs& gs, Ts&&... xs) {
    actor_config cfg;
    return spawn_class_in_groups<T, Os>(cfg, gs.begin(), gs.end(),
                                        std::forward<Ts>(xs)...);
  }

  /// Returns a new class-based actor subscribed to all groups in `gs`.
  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  infer_handle_from_class_t<T>
  spawn_in_group(const group& grp, Ts&&... xs) {
    return spawn_in_groups<T, Os>({grp}, std::forward<Ts>(xs)...);
  }

  /// Returns whether this actor system calls `await_all_actors_done`
  /// in its destructor before shutting down.
  inline bool await_actors_before_shutdown() const {
    return await_actors_before_shutdown_;
  }

  /// Configures whether this actor system calls `await_all_actors_done`
  /// in its destructor before shutting down.
  inline void await_actors_before_shutdown(bool x) {
    await_actors_before_shutdown_ = x;
  }

  /// Returns the configuration of this actor system.
  const actor_system_config& config() const {
    return cfg_;
  }

    /// Returns configuration parameters for individual named actors types.
  inline const named_actor_config_map& named_actor_configs() const {
    return named_actor_configs_;
  }

  /// @cond PRIVATE

  /// Increases running-detached-threads-count by one.
  void inc_detached_threads();

  /// Decreases running-detached-threads-count by one.
  void dec_detached_threads();

  /// Blocks the caller until all detached threads are done.
  void await_detached_threads();

  /// @endcond

private:
  template <class T>
  void check_invariants() {
    static_assert(!std::is_base_of<prohibit_top_level_spawn_marker, T>::value,
                  "This actor type cannot be spawned throught an actor system. "
                  "Probably you have tried to spawn a broker or opencl actor.");
  }

  expected<strong_actor_ptr> dyn_spawn_impl(const std::string& name,
                                            message& args,
                                            execution_unit* ctx,
                                            bool check_interface,
                                            optional<const mpi&> expected_ifs);

  template <class C, spawn_options Os, class... Ts>
  infer_handle_from_class_t<C>
  spawn_impl(actor_config& cfg, Ts&&... xs) {
    static_assert(is_unbound(Os),
                  "top-level spawns cannot have monitor or link flag");
    cfg.flags = has_priority_aware_flag(Os)
                ? abstract_actor::is_priority_aware_flag
                : 0;
    if (has_detach_flag(Os) || std::is_base_of<blocking_actor, C>::value)
      cfg.flags |= abstract_actor::is_detached_flag;
    if (!cfg.host)
      cfg.host = dummy_execution_unit();
    CAF_SET_LOGGER_SYS(this);
    auto res = make_actor<C>(next_actor_id(), node(), this,
                             cfg, std::forward<Ts>(xs)...);
    auto ptr = static_cast<C*>(actor_cast<abstract_actor*>(res));
    ptr->launch(cfg.host, has_lazy_init_flag(Os), has_hide_flag(Os));
    return res;
  }

  /// Sets the internal actor for dynamic spawn operations.
  inline void spawn_serv(strong_actor_ptr x) {
    internal_actors_[internal_actor_id(atom("SpawnServ"))] = std::move(x);
  }

  /// Sets the internal actor for storing the runtime configuration.
  inline void config_serv(strong_actor_ptr x) {
    internal_actors_[internal_actor_id(atom("ConfigServ"))] = std::move(x);
  }

  /// Sets the internal actor for managing streams that
  /// cross network boundaries. Called in middleman::start.
  void stream_serv(strong_actor_ptr x);

  std::atomic<size_t> ids_;
  uniform_type_info_map types_;
  node_id node_;
  intrusive_ptr<caf::logger> logger_;
  actor_registry registry_;
  group_manager groups_;
  module_array modules_;
  scoped_execution_unit dummy_execution_unit_;
  bool await_actors_before_shutdown_;
  // Stores SpawnServ, ConfigServ, and StreamServ
  std::array<strong_actor_ptr, num_internal_actors> internal_actors_;
  std::atomic<size_t> detached;
  mutable std::mutex detached_mtx;
  mutable std::condition_variable detached_cv;
  actor_system_config& cfg_;
  std::mutex logger_dtor_mtx_;
  std::condition_variable logger_dtor_cv_;
  volatile bool logger_dtor_done_;
  named_actor_config_map named_actor_configs_;
};

} // namespace caf

#endif // CAF_ACTOR_SYSTEM_HPP
