/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_MIXIN_FUNCTOR_BASED_HPP
#define CAF_MIXIN_FUNCTOR_BASED_HPP

namespace caf {
namespace mixin {

template <class Base, class Subtype>
class functor_based : public Base {
 public:
  using combined_type = functor_based;

  using pointer = Base*;

  using make_behavior_fun = std::function<behavior(pointer)>;

  using void_fun = std::function<void(pointer)>;

  functor_based() {
    // nop
  }

  template <class F, class... Ts>
  functor_based(F f, Ts&&... vs) {
    init(std::move(f), std::forward<Ts>(vs)...);
  }

  template <class F, class... Ts>
  void init(F f, Ts&&... vs) {
    using trait = typename detail::get_callable_trait<F>::type;
    using arg_types = typename trait::arg_types;
    using result_type = typename trait::result_type;
    constexpr bool returns_behavior =
      std::is_convertible<result_type, behavior>::value;
    constexpr bool uses_first_arg = std::is_same<
      typename detail::tl_head<arg_types>::type, pointer>::value;
    std::integral_constant<bool, returns_behavior> token1;
    std::integral_constant<bool, uses_first_arg> token2;
    set(token1, token2, std::move(f), std::forward<Ts>(vs)...);
  }

 protected:
  make_behavior_fun m_make_behavior;

 private:
  template <class F>
  void set(std::true_type, std::true_type, F&& fun) {
    // behavior (pointer)
    m_make_behavior = std::forward<F>(fun);
  }

  template <class F>
  void set(std::false_type, std::true_type, F fun) {
    // void (pointer)
    m_make_behavior = [fun](pointer ptr) {
      fun(ptr);
      return behavior{};

    };
  }

  template <class F>
  void set(std::true_type, std::false_type, F fun) {
    // behavior (void)
    m_make_behavior = [fun](pointer) { return fun(); };
  }

  template <class F>
  void set(std::false_type, std::false_type, F fun) {
    // void (void)
    m_make_behavior = [fun](pointer) {
      fun();
      return behavior{};

    };
  }

  template <class Token, typename F, typename T0, class... Ts>
  void set(Token t1, std::true_type t2, F fun, T0&& arg0, Ts&&... args) {
    set(t1, t2,
      std::bind(fun, std::placeholders::_1, std::forward<T0>(arg0),
            std::forward<Ts>(args)...));
  }

  template <class Token, typename F, typename T0, class... Ts>
  void set(Token t1, std::false_type t2, F fun, T0&& arg0, Ts&&... args) {
    set(t1, t2,
      std::bind(fun, std::forward<T0>(arg0), std::forward<Ts>(args)...));
  }
};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_FUNCTOR_BASED_HPP
