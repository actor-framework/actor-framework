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

#ifndef CAF_TYPED_BEHAVIOR_HPP
#define CAF_TYPED_BEHAVIOR_HPP

#include "caf/either.hpp"
#include "caf/behavior.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_continue_helper.hpp"

#include "caf/detail/ctm.hpp"
#include "caf/detail/typed_actor_util.hpp"

namespace caf {

namespace detail {

// converts a list of replies_to<...>::with<...> elements to a list of
// lists containing the replies_to<...> half only
template <class List>
struct input_only;

template <class... Ts>
struct input_only<detail::type_list<Ts...>> {
  using type = detail::type_list<typename Ts::input_types...>;
};

using skip_list = detail::type_list<skip_message_t>;

template <class Opt1, class Opt2>
struct collapse_replies_to_statement {
  using type = typename either<Opt1>::template or_else<Opt2>;
};

template <class List>
struct collapse_replies_to_statement<List, none_t> {
  using type = List;
};

template <class Input, class RepliesToWith>
struct same_input : std::is_same<Input, typename RepliesToWith::input_types> {};

template <class Output, class RepliesToWith>
struct same_output_or_skip_message_t {
  using other = typename RepliesToWith::output_types;
  static constexpr bool value =
    std::is_same<Output, typename RepliesToWith::output_types>::value ||
    std::is_same<Output, type_list<skip_message_t>>::value;
};

template <class SList>
struct valid_input_predicate {
  template <class Expr>
  struct inner {
    using input_types = typename Expr::input_types;
    using output_types =
      typename collapse_replies_to_statement<
        typename Expr::output_opt1_types,
        typename Expr::output_opt2_types
      >::type;
    // get matching elements for input type
    using filtered_slist =
      typename tl_filter<
        SList,
        tbind<same_input, input_types>::template type
      >::type;
    static_assert(tl_size<filtered_slist>::value > 0,
                  "cannot assign given match expression to "
                  "typed behavior, because the expression "
                  "contains at least one pattern that is "
                  "not defined in the actor's type");
    static constexpr bool value = tl_exists<
      filtered_slist, tbind<same_output_or_skip_message_t,
                            output_types>::template type>::value;
    // check whether given output matches in the filtered list
    static_assert(value,
                  "cannot assign given match expression to "
                  "typed behavior, because at least one return "
                  "type does not match");
  };
};

template <class T>
struct is_system_msg_handler : std::false_type { };

template <>
struct is_system_msg_handler<reacts_to<exit_msg>> : std::true_type { };

template <>
struct is_system_msg_handler<reacts_to<down_msg>> : std::true_type { };

// Tests whether the input list (IList) matches the
// signature list (SList) for a typed actor behavior
template <class SList, class IList>
struct valid_input {
  // strip exit_msg and down_msg from input types,
  // because they're always allowed
  using adjusted_slist =
    typename tl_filter_not<
      SList,
      is_system_msg_handler
    >::type;
  using adjusted_ilist =
    typename tl_filter_not<
      IList,
      is_system_msg_handler
    >::type;
  // check for each element in IList that there's an element in SList that
  // (1) has an identical input type list
  // (2)  has an identical output type list
  //   OR the output of the element in IList is skip_message_t
  static_assert(detail::tl_is_distinct<IList>::value,
                "given pattern is not distinct");
  static constexpr bool value =
    tl_size<adjusted_slist>::value == tl_size<adjusted_ilist>::value
    && tl_forall<
         adjusted_ilist,
         valid_input_predicate<adjusted_slist>::template inner
       >::value;
};

// this function is called from typed_behavior<...>::set and its whole
// purpose is to give users a nicer error message on a type mismatch
// (this function only has the type informations needed to understand the error)
template <class SignatureList, class InputList>
void static_check_typed_behavior_input() {
  constexpr bool is_valid = valid_input<SignatureList, InputList>::value;
  // note: it might be worth considering to allow a wildcard in the
  //     InputList if its return type is identical to all "missing"
  //     input types ... however, it might lead to unexpected results
  //     and would cause a lot of not-so-straightforward code here
  static_assert(is_valid,
                "given pattern cannot be used to initialize "
                "typed behavior (exact match needed)");
}

} // namespace detail

template <class... Sigs>
class typed_actor;

namespace mixin {
template <class, class, class>
class behavior_stack_based_impl;
}

template <class... Sigs>
class typed_behavior {
public:
  template <class... OtherSigs>
  friend class typed_actor;

  template <class, class, class>
  friend class mixin::behavior_stack_based_impl;

  typed_behavior(typed_behavior&&) = default;
  typed_behavior(const typed_behavior&) = default;
  typed_behavior& operator=(typed_behavior&&) = default;
  typed_behavior& operator=(const typed_behavior&) = default;

  using signatures = detail::type_list<Sigs...>;

  template <class T, class... Ts>
  typed_behavior(T x, Ts... xs) {
    set(detail::make_behavior(x, xs...));
  }

  explicit operator bool() const {
    return static_cast<bool>(bhvr_);
  }

   /// Invokes the timeout callback.
  void handle_timeout() {
    bhvr_.handle_timeout();
  }

  /// Returns the duration after which receives using
  /// this behavior should time out.
  const duration& timeout() const {
    return bhvr_.timeout();
  }

  /// @cond PRIVATE

  behavior& unbox() {
    return bhvr_;
  }

  static typed_behavior make_empty_behavior() {
    return {};
  }

  /// @endcond

private:
  typed_behavior() = default;

  template <class... Ts>
  void set(intrusive_ptr<detail::default_behavior_impl<std::tuple<Ts...>>> bp) {
    using mpi =
      typename detail::tl_filter_not<
        detail::type_list<typename detail::deduce_mpi<Ts>::type...>,
        detail::is_hidden_msg_handler
      >::type;
    detail::static_asserter<signatures, mpi, detail::ctm>::verify_match();
    // final (type-erasure) step
    intrusive_ptr<detail::behavior_impl> ptr = std::move(bp);
    bhvr_.assign(std::move(ptr));
  }

  behavior bhvr_;
};

} // namespace caf

#endif // CAF_TYPED_BEHAVIOR_HPP
