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

#include <tuple>
#include <functional>

#include "caf/fwd.hpp"

#include "caf/detail/apply_args.hpp"
#include "caf/detail/spawn_fwd.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

template <class Base, class F, class ArgsPtr,
          bool ReturnsBehavior, bool HasSelfPtr>
class init_fun_factory_helper {
public:
  init_fun_factory_helper(init_fun_factory_helper&&) = default;
  init_fun_factory_helper(const init_fun_factory_helper&) = default;

  init_fun_factory_helper(F fun, ArgsPtr args)
      : fun_(std::move(fun)),
        args_(std::move(args)) {
    // nop
  }

  behavior operator()(local_actor* self) {
    bool_token<ReturnsBehavior> returns_behavior_token;
    bool_token<HasSelfPtr> captures_self_token;
    return apply(returns_behavior_token, captures_self_token, self);
  }

private:
  // behavior (pointer)
  behavior apply(std::true_type, std::true_type, local_actor* ptr) {
    auto res = apply_moved_args_prefixed(fun_, get_indices(*args_), *args_,
                                         static_cast<Base*>(ptr));
    return std::move(res.unbox());
  }

  // void (pointer)
  behavior apply(std::false_type, std::true_type, local_actor* ptr) {
    apply_moved_args_prefixed(fun_, get_indices(*args_),
                              *args_, static_cast<Base*>(ptr));
    return behavior{};
  }

  // behavior ()
  behavior apply(std::true_type, std::false_type, local_actor*) {
    auto res = apply_args(fun_, get_indices(*args_), *args_);
    return std::move(res.unbox());
  }

  // void ()
  behavior apply(std::false_type, std::false_type, local_actor*) {
    apply_args(fun_, get_indices(*args_), *args_);
    return behavior{};
  }

  F fun_;
  ArgsPtr args_;
};

template <class Base, class F>
class init_fun_factory {
public:
  using fun = std::function<behavior (local_actor*)>;

  template <class... Ts>
  fun operator()(F f, Ts&&... xs) {
    static_assert(std::is_base_of<local_actor, Base>::value,
                  "Given Base does not extend local_actor");
    using trait = typename detail::get_callable_trait<F>::type;
    using arg_types = typename trait::arg_types;
    using res_type = typename trait::result_type;
    using first_arg = typename detail::tl_head<arg_types>::type;
    constexpr bool selfptr = std::is_pointer<first_arg>::value;
    constexpr bool rets = std::is_convertible<res_type, behavior>::value;
    using tuple_type = decltype(std::make_tuple(detail::spawn_fwd<Ts>(xs)...));
    using tuple_ptr = std::shared_ptr<tuple_type>;
    using helper = init_fun_factory_helper<Base, F, tuple_ptr, rets, selfptr>;
    return helper{std::move(f),
                  sizeof...(Ts) > 0
                  ? std::make_shared<tuple_type>(detail::spawn_fwd<Ts>(xs)...)
                  : nullptr};
  }
};

} // namespace detail
} // namespace caf

