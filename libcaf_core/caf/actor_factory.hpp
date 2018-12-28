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

#include <set>
#include <string>

#include "caf/actor_addr.hpp"
#include "caf/actor_system.hpp"
#include "caf/infer_handle.hpp"
#include "caf/execution_unit.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

using actor_factory_result = std::pair<strong_actor_ptr, std::set<std::string>>;

using actor_factory = std::function<actor_factory_result (actor_config&, message&)>;

using selfptr_mode_token = spawn_mode_token<spawn_mode::function_with_selfptr>;

using void_mode_token = spawn_mode_token<spawn_mode::function>;

template <class F, class T, class Bhvr, spawn_mode Mode, class R, class Sig>
class fun_decorator;

template <class F, class T, class Bhvr, class R, class... Ts>
class fun_decorator<F, T, Bhvr, spawn_mode::function,
                    R, detail::type_list<Ts...>> {
public:
  fun_decorator(F  f, T*) : f_(std::move(f)) {
    // nop
  }

  behavior operator()(Ts... xs) {
    detail::type_list<R> token;
    return apply(token, xs...);
  }

  template <class U>
  typename std::enable_if<
    std::is_convertible<U, Bhvr>::value,
    behavior
  >::type
  apply(detail::type_list<U>, Ts... xs) {
    auto bhvr = f_(xs...);
    return std::move(bhvr.unbox());
  }

  template <class U>
  typename std::enable_if<
    !std::is_convertible<U, Bhvr>::value,
    behavior
  >::type
  apply(detail::type_list<U>, Ts... xs) {
    f_(xs...);
    return {};
  }

private:
  F f_;
};

template <class F, class T, class Bhvr, class R, class... Ts>
class fun_decorator<F, T, Bhvr, spawn_mode::function_with_selfptr,
                    R, detail::type_list<T*, Ts...>> {
public:
  fun_decorator(F  f, T* ptr) : f_(std::move(f)), ptr_(ptr) {
    // nop
  }

  behavior operator()(Ts... xs) {
    detail::type_list<R> token;
    return apply(token, xs...);
  }

  template <class U>
  typename std::enable_if<
    std::is_convertible<U, Bhvr>::value,
    behavior
  >::type
  apply(detail::type_list<U>, Ts... xs) {
    auto bhvr = f_(ptr_, xs...);
    return std::move(bhvr.unbox());
  }

  template <class U>
  typename std::enable_if<
    !std::is_convertible<U, Bhvr>::value,
    behavior
  >::type
  apply(detail::type_list<U>, Ts... xs) {
    f_(ptr_, xs...);
    return {};
  }

private:
  F f_;
  T* ptr_;
};

template <class Args>
struct message_verifier;

template <>
struct message_verifier<detail::type_list<>> {
  bool operator()(message& msg, void_mode_token) {
    return msg.empty();
  }
};

template <class T, class... Ts>
struct message_verifier<detail::type_list<T, Ts...>> {
  bool operator()(message& msg, void_mode_token) {
    return msg.match_elements<T, Ts...>();
  }
  bool operator()(message& msg, selfptr_mode_token) {
    return msg.match_elements<Ts...>();
  }
};

template <class F>
actor_factory make_actor_factory(F fun) {
  return [fun](actor_config& cfg, message& msg) -> actor_factory_result {
    using trait = infer_handle_from_fun<F>;
    using handle = typename trait::type;
    using impl = typename trait::impl;
    using behavior_t = typename trait::behavior_type;
    spawn_mode_token<trait::mode> tk;
    message_verifier<typename trait::arg_types> mv;
    if (!mv(msg, tk))
      return {};
    cfg.init_fun = actor_config::init_fun_type{[=](local_actor* x) -> behavior {
      using ctrait = typename detail::get_callable_trait<F>::type;
      using fd = fun_decorator<F, impl, behavior_t, trait::mode,
                               typename ctrait::result_type,
                               typename ctrait::arg_types>;
      fd f{fun, static_cast<impl*>(x)};
      empty_type_erased_tuple dummy_;
      auto& ct = msg.empty() ? dummy_ : const_cast<message&>(msg).content();
      auto opt = ct.apply(f);
      if (!opt)
        return {};
      return std::move(*opt);
    }};
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
  using handle = typename infer_handle_from_class<T>::type;
  handle hdl;
  dyn_spawn_class_helper<handle, T, Ts...> factory{hdl, cfg};
  msg.apply(factory);
  return {actor_cast<strong_actor_ptr>(std::move(hdl)),
          cfg.host->system().message_types<handle>()};
}

template <class T, class... Ts>
actor_factory make_actor_factory() {
  static_assert(detail::conjunction<
                  std::is_lvalue_reference<Ts>::value...
                >::value,
                "all Ts must be lvalue references");
  static_assert(std::is_base_of<local_actor, T>::value,
                "T is not derived from local_actor");
  return &dyn_spawn_class<T, Ts...>;
}

} // namespace caf

