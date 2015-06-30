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

#ifndef CAF_DETAIL_INIT_FUN_FACTORY_HPP
#define CAF_DETAIL_INIT_FUN_FACTORY_HPP

#include <functional>

#include "caf/fwd.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

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
    using result_type = typename trait::result_type;
    using first_arg = typename detail::tl_head<arg_types>::type;
    bool_token<std::is_convertible<result_type, behavior>::value> token1;
    bool_token<std::is_pointer<first_arg>::value> token2;
    return make(token1, token2, std::move(f), std::forward<Ts>(xs)...);
  }

private:
  // behavior (pointer)
  template <class Fun>
  static fun make(std::true_type, std::true_type, Fun fun) {
    return [fun](local_actor* ptr) -> behavior {
      auto res = fun(static_cast<Base*>(ptr));
      return std::move(res.unbox());
    };
  }

  // void (pointer)
  template <class Fun>
  static fun make(std::false_type, std::true_type, Fun fun) {
    return [fun](local_actor* ptr) -> behavior {
      fun(static_cast<Base*>(ptr));
      return behavior{};
    };
  }

  // behavior ()
  template <class Fun>
  static fun make(std::true_type, std::false_type, Fun fun) {
    return [fun](local_actor*) -> behavior {
      auto res = fun();
      return std::move(res.unbox());
    };
  }

  // void ()
  template <class Fun>
  static fun make(std::false_type, std::false_type, Fun fun) {
    return [fun](local_actor*) -> behavior {
      fun();
      return behavior{};
    };
  }

  template <class Token, typename T0, class... Ts>
  static fun make(Token t1, std::true_type t2, F fun, T0&& x, Ts&&... xs) {
    return make(t1, t2,
                std::bind(fun, std::placeholders::_1, detail::spawn_fwd<T0>(x),
                          detail::spawn_fwd<Ts>(xs)...));
  }

  template <class Token, typename T0, class... Ts>
  static fun make(Token t1, std::false_type t2, F fun, T0&& x, Ts&&... xs) {
    return make(t1, t2, std::bind(fun, detail::spawn_fwd<T0>(x),
                                  detail::spawn_fwd<Ts>(xs)...));
  }
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_INIT_FUN_FACTORY_HPP
