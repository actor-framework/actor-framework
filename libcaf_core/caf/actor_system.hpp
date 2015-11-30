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

#ifndef CAF_ACTOR_SYSTEM_HPP
#define CAF_ACTOR_SYSTEM_HPP

#include <atomic>
#include <string>
#include <memory>
#include <functional>

#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/infer_handle.hpp"
#include "caf/actor_config.hpp"
#include "caf/make_counted.hpp"
#include "caf/spawn_options.hpp"
#include "caf/group_manager.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/actor_registry.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/scoped_execution_unit.hpp"
#include "caf/uniform_type_info_map.hpp"
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
    auto nr = detail::type_nr<T>::value;
    if (nr != 0)
      return *types.portable_name(nr, nullptr);
    auto ptr = types.portable_name(0, &typeid(T));
    if (ptr)
      return *ptr;
    return "INVALID-TYPE";
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

template <class... Is, class... Ls, class... Rs>
struct typed_mpi_access<typed_mpi<detail::type_list<Is...>,
                                  detail::type_list<Ls...>,
                                  detail::type_list<Rs...>>> {
  std::string operator()(const uniform_type_info_map& types) const {
    static_assert(sizeof...(Is) > 0, "typed MPI without inputs");
    static_assert(sizeof...(Ls) > 0, "typed MPI without outputs");
    std::vector<std::string> inputs{get_mpi_field<Is>(types)...};
    std::vector<std::string> outputs1{get_mpi_field<Ls>(types)...};
    std::vector<std::string> outputs2{get_mpi_field<Rs>(types)...};
    std::string result = "caf::replies_to<";
    result += join(inputs, ",");
    if (outputs2.empty()) {
      result += ">::with<";
      result += join(outputs1, ",");
    } else {
      result += ">::with_either<";
      result += join(outputs1, ",");
      result += ">::or_else<";
      result += join(outputs2, ",");
    }
    result += ">";
    return result;
  }
};

template <class T>
std::string get_rtti_from_mpi(const uniform_type_info_map& types) {
  typed_mpi_access<T> f;
  return f(types);
}

///
class actor_system {
public:
  friend class abstract_actor;
  friend class io::middleman;

  actor_system(const actor_system&) = delete;
  actor_system& operator=(const actor_system&) = delete;

  /// An (optional) component of the actor system.
  class module {
  public:
    enum id_t {
      scheduler,
      middleman,
      opencl_manager,
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

  actor_system();

  actor_system(int argc, char** argv);

  explicit actor_system(actor_system_config& cfg);

  explicit actor_system(actor_system_config&& cfg);

  virtual ~actor_system();

  using message_types_set = std::set<std::string>;

  inline message_types_set message_types(const actor&) {
    return message_types_set{};
  }

  /// Returns a string representation of the messaging
  /// interface using portable names;
  template <class... Ts>
  message_types_set message_types(const typed_actor<Ts...>&) {
    static_assert(sizeof...(Ts) > 0, "empty typed actor handle given");
    message_types_set result{get_rtti_from_mpi<Ts>(types())...};
    return result;
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
  uniform_type_info_map& types();

  /// Returns the system-wide group manager.
  group_manager& groups();

  /// Returns `true` if the I/O module is available, `false` otherwise.
  bool has_middleman() const;

  /// Returns the middleman instance from the I/O module.
  /// @throws `std::logic_error` if the I/O module has not been loaded.
  io::middleman& middleman();

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
  typename infer_handle_from_class<C>::type
  spawn_class(actor_config& cfg, Ts&&... xs) {
    return spawn_impl<C, Os>(cfg, detail::spawn_fwd<Ts>(xs)...);
  }

  /// Returns a new actor of type `C` using `xs...` as constructor
  /// arguments. The behavior of `spawn` can be modified by setting `Os`, e.g.,
  /// to opt-out of the cooperative scheduling.
  /// @param xs Constructor arguments for `C`.
  template <class C, spawn_options Os = no_spawn_options, class... Ts>
  typename infer_handle_from_class<C>::type spawn(Ts&&... xs) {
    check_invariants<C>();
    actor_config cfg;
    return spawn_impl<C, Os>(cfg, detail::spawn_fwd<Ts>(xs)...);
  }

  /// Called by `spawn_functor` to apply static assertions and
  /// store an initialization function in `cfg` before calling `spawn_class`.
  /// @param cfg To-be-filled config for the actor.
  /// @param fun Function object for the actor's behavior; will be moved.
  /// @param xs Arguments for `fun`.
  template <spawn_options Os, class C, class F, class... Ts>
  typename infer_handle_from_class<C>::type
  spawn_functor_impl(actor_config& cfg, F& fun, Ts&&... xs) {
    constexpr bool has_blocking_base =
      std::is_base_of<blocking_actor, C>::value;
    static_assert(has_blocking_base || ! has_blocking_api_flag(Os),
                  "blocking functor-based actors "
                  "need to be spawned using the blocking_api flag");
    static_assert(! has_blocking_base || has_blocking_api_flag(Os),
                  "non-blocking functor-based actors "
                  "cannot be spawned using the blocking_api flag");
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
  typename infer_handle_from_fun<F>::type
  spawn_functor(actor_config& cfg, F& fun, Ts&&... xs) {
    using impl = typename infer_handle_from_fun<F>::impl;
    return spawn_functor_impl<Os, impl>(cfg, fun, std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based actor. The first argument must be the functor,
  /// the remainder of `xs...` is used to invoke the functor.
  /// The behavior of `spawn` can be modified by setting `Os`, e.g.,
  /// to opt-out of the cooperative scheduling.
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn(F fun, Ts&&... xs) {
    check_invariants<typename infer_handle_from_fun<F>::impl>();
    actor_config cfg;
    return spawn_functor<Os>(cfg, fun, std::forward<Ts>(xs)...);
  }

  template <class T, spawn_options Os = no_spawn_options, class Iter, class F, class... Ts>
  typename infer_handle_from_class<T>::type
  spawn_in_groups_impl(actor_config& cfg, Iter first, Iter second, Ts&&... xs) {
    check_invariants<T>();
    auto irange = make_input_range(first, second);
    cfg.groups = &irange;
    return spawn_class<T, Os>(cfg, std::forward<Ts>(xs)...);
  }

  template <spawn_options Os = no_spawn_options, class Iter, class F, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn_in_groups_impl(actor_config& cfg, Iter first, Iter second,
                       F& fun, Ts&&... xs) {
    check_invariants<typename infer_handle_from_fun<F>::impl>();
    auto irange = make_input_range(first, second);
    cfg.groups = &irange;
    return spawn_functor<Os>(cfg, fun, std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based actor subscribed to all groups in `gs`.
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn_in_groups(std::initializer_list<group> gs, F fun, Ts&&... xs) {
    actor_config cfg;
    return spawn_in_groups_impl(cfg, gs.begin(), gs.end(), fun,
                                std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based actor subscribed to all groups in `gs`.
  template <spawn_options Os = no_spawn_options, class Gs, class F, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn_in_groups(const Gs& gs, F fun, Ts&&... xs) {
    actor_config cfg;
    return spawn_in_groups_impl(cfg, gs.begin(), gs.end(), fun,
                                std::forward<Ts>(xs)...);
  }

  /// Returns a new functor-based actor subscribed to all groups in `gs`.
  template <spawn_options Os = no_spawn_options, class F, class... Ts>
  typename infer_handle_from_fun<F>::type
  spawn_in_group(const group& grp, F fun, Ts&&... xs) {
    return spawn_in_groups({grp}, std::move(fun), std::forward<Ts>(xs)...);
  }

  /// Returns a new class-based actor subscribed to all groups in `gs`.
  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  typename infer_handle_from_class<T>::type
  spawn_in_groups(std::initializer_list<group> gs, Ts&&... xs) {
    actor_config cfg;
    return spawn_in_groups_impl<T>(cfg, gs.begin(), gs.end(),
                                   std::forward<Ts>(xs)...);
  }

  /// Returns a new class-based actor subscribed to all groups in `gs`.
  template <class T, spawn_options Os = no_spawn_options, class Gs, class... Ts>
  typename infer_handle_from_class<T>::type
  spawn_in_groups(const Gs& gs, Ts&&... xs) {
    actor_config cfg;
    return spawn_in_groups_impl<T>(cfg, gs.begin(), gs.end(),
                                   std::forward<Ts>(xs)...);
  }

  /// Returns a new class-based actor subscribed to all groups in `gs`.
  template <class T, spawn_options Os = no_spawn_options, class... Ts>
  typename infer_handle_from_class<T>::type
  spawn_in_group(const group& grp, Ts&&... xs) {
    return spawn_in_groups<T>({grp}, std::forward<Ts>(xs)...);
  }

  /// @cond PRIVATE
  inline atom_value backend_name() const {
    return backend_name_;
  }
  /// @endcond

private:
  template <class T>
  void check_invariants() {
    static_assert(! std::is_base_of<prohibit_top_level_spawn_marker, T>::value,
                  "This actor type cannot be spawned throught an actor system. "
                  "Probably you have tried to spawn a broker or opencl actor.");
  }

  template <class C, spawn_options Os, class... Ts>
  typename infer_handle_from_class<C>::type
  spawn_impl(actor_config& cfg, Ts&&... xs) {
    static_assert(! std::is_base_of<blocking_actor, C>::value
                  || has_blocking_api_flag(Os),
                  "C is derived from blocking_actor but "
                  "spawned without blocking_api_flag");
    static_assert(is_unbound(Os),
                  "top-level spawns cannot have monitor or link flag");
    cfg.flags = has_priority_aware_flag(Os)
                ? abstract_actor::is_priority_aware_flag
                : 0;
    if (has_detach_flag(Os) || has_blocking_api_flag(Os))
      cfg.flags |= abstract_actor::is_detached_flag;
    if (! cfg.host)
      cfg.host = dummy_execution_unit();
    auto ptr = make_counted<C>(cfg, std::forward<Ts>(xs)...);
    CAF_SET_LOGGER_SYS(this);
    CAF_LOG_DEBUG("spawned actor:" << CAF_ARG(ptr->id()));
    CAF_PUSH_AID(ptr->id());
    ptr->launch(cfg.host, has_lazy_init_flag(Os), has_hide_flag(Os));
    return ptr;
  }

  std::atomic<actor_id> ids_;
  uniform_type_info_map types_;
  node_id node_;
  caf::logger logger_;
  actor_registry registry_;
  group_manager groups_;
  module_array modules_;
  io::middleman* middleman_;
  scoped_execution_unit dummy_execution_unit_;
  atom_value backend_name_;
};

} // namespace caf

#endif //CAF_ACTOR_SYSTEM_HPP
