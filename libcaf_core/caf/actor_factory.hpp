// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_addr.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/execution_unit.hpp"
#include "caf/infer_handle.hpp"

#include <set>
#include <string>

namespace caf {

using actor_factory_result = std::pair<strong_actor_ptr, std::set<std::string>>;

using actor_factory
  = std::function<actor_factory_result(actor_config&, message&)>;

using selfptr_mode_token = spawn_mode_token<spawn_mode::function_with_selfptr>;

using void_mode_token = spawn_mode_token<spawn_mode::function>;

template <class F, class T, class Bhvr, spawn_mode Mode, class R, class Sig>
class fun_decorator;

template <class F, class T, class Bhvr, class R, class... Ts>
class fun_decorator<F, T, Bhvr, spawn_mode::function, R,
                    detail::type_list<Ts...>> {
public:
  fun_decorator(F f, T*, behavior* bhvr) : f_(std::move(f)), bhvr_(bhvr) {
    // nop
  }

  void operator()(Ts... xs) {
    if constexpr (std::is_convertible_v<R, Bhvr>) {
      auto bhvr = f_(xs...);
      *bhvr_ = std::move(bhvr.unbox());
    } else {
      f_(xs...);
    }
  }

private:
  F f_;
  behavior* bhvr_;
};

template <class F, class T, class Bhvr, class R, class... Ts>
class fun_decorator<F, T, Bhvr, spawn_mode::function_with_selfptr, R,
                    detail::type_list<T*, Ts...>> {
public:
  fun_decorator(F f, T* ptr, behavior* bhvr)
    : f_(std::move(f)), ptr_(ptr), bhvr_(bhvr) {
    // nop
  }

  void operator()(Ts... xs) {
    if constexpr (std::is_convertible_v<R, Bhvr>) {
      auto bhvr = f_(ptr_, xs...);
      *bhvr_ = std::move(bhvr.unbox());
    } else {
      f_(ptr_, xs...);
    }
  }

private:
  F f_;
  T* ptr_;
  behavior* bhvr_;
};

template <spawn_mode Mode, class Args>
struct message_verifier;

template <class... Ts>
struct message_verifier<spawn_mode::function, detail::type_list<Ts...>> {
  bool operator()(message& msg) {
    return msg.types() == make_type_id_list<Ts...>();
  }
};

template <class Self, class... Ts>
struct message_verifier<spawn_mode::function_with_selfptr,
                        detail::type_list<Self*, Ts...>> {
  bool operator()(message& msg) {
    return msg.types() == make_type_id_list<Ts...>();
  }
};

template <class F>
actor_factory make_actor_factory(F fun) {
  return [fun](actor_config& cfg, message& msg) -> actor_factory_result {
    using trait = infer_handle_from_fun<F>;
    using handle = typename trait::type;
    using impl = typename trait::impl;
    using behavior_t = typename trait::behavior_type;
    message_verifier<trait::mode, typename trait::arg_types> verify;
    if (!verify(msg))
      return {};
    cfg.init_fun = actor_config::init_fun_type{
      [=](local_actor* x) mutable -> behavior {
        using ctrait = detail::get_callable_trait_t<F>;
        using fd = fun_decorator<F, impl, behavior_t, trait::mode,
                                 typename ctrait::result_type,
                                 typename ctrait::arg_types>;
        behavior result;
        message_handler f{fd{fun, static_cast<impl*>(x), &result}};
        f(msg);
        return result;
      },
    };
    handle hdl = cfg.host->system().spawn_class<impl, no_spawn_options>(cfg);
    return {actor_cast<strong_actor_ptr>(std::move(hdl)),
            cfg.host->system().message_types<handle>()};
  };
}

template <class Handle, class T, class... Ts>
struct dyn_spawn_class_helper {
  Handle& result;
  actor_config& cfg;
  void operator()(Ts... xs) {
    CAF_ASSERT(cfg.host);
    result = cfg.host->system().spawn_class<T, no_spawn_options>(cfg, xs...);
  }
};

template <class T, class... Ts>
actor_factory_result dyn_spawn_class(actor_config& cfg, message& msg) {
  CAF_ASSERT(cfg.host);
  using handle = infer_handle_from_class_t<T>;
  handle hdl;
  message_handler factory{dyn_spawn_class_helper<handle, T, Ts...>{hdl, cfg}};
  factory(msg);
  return {actor_cast<strong_actor_ptr>(std::move(hdl)),
          cfg.host->system().message_types<handle>()};
}

template <class T, class... Ts>
actor_factory make_actor_factory() {
  static_assert(detail::conjunction_v<std::is_lvalue_reference_v<Ts>...>,
                "all Ts must be lvalue references");
  static_assert(std::is_base_of_v<local_actor, T>,
                "T is not derived from local_actor");
  return &dyn_spawn_class<T, Ts...>;
}

} // namespace caf
