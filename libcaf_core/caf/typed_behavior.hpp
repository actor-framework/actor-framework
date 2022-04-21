// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/behavior.hpp"
#include "caf/deduce_mpi.hpp"
#include "caf/interface_mismatch.hpp"
#include "caf/message_handler.hpp"
#include "caf/system_messages.hpp"
#include "caf/timespan.hpp"
#include "caf/unsafe_behavior_init.hpp"

#include "caf/detail/typed_actor_util.hpp"

namespace caf ::detail {

template <class Signature>
struct input_args;

template <class R, class... Ts>
struct input_args<R(Ts...)> {
  using type = type_list<Ts...>;
};

// Converts a list of function signatures to lists of inputs (dropping the
// return type).
template <class List>
struct input_only;

template <class... Ts>
struct input_only<type_list<Ts...>> {
  using type = type_list<typename input_args<Ts>::type...>;
};

using skip_list = detail::type_list<skip_t>;

template <class Input, class RepliesToWith>
struct same_input : std::is_same<Input, typename RepliesToWith::input_types> {};

template <class Output, class RepliesToWith>
struct same_output_or_skip_t {
  using other = typename RepliesToWith::output_types;
  static constexpr bool value = std::is_same<
                                  Output,
                                  typename RepliesToWith::output_types>::value
                                || std::is_same<Output,
                                                type_list<skip_t>>::value;
};

template <class SList>
struct valid_input_predicate {
  template <class Expr>
  struct inner {
    using input_types = typename Expr::input_types;
    using output_types = typename Expr::output_types;
    // get matching elements for input type
    using filtered_slist = typename tl_filter<
      SList, tbind<same_input, input_types>::template type>::type;
    static_assert(tl_size<filtered_slist>::value > 0,
                  "cannot assign given match expression to "
                  "typed behavior, because the expression "
                  "contains at least one pattern that is "
                  "not defined in the actor's type");
    static constexpr bool value = tl_exists<
      filtered_slist,
      tbind<same_output_or_skip_t, output_types>::template type>::value;
    // check whether given output matches in the filtered list
    static_assert(value, "cannot assign given match expression to "
                         "typed behavior, because at least one return "
                         "type does not match");
  };
};

template <class T>
struct is_system_msg_handler : std::false_type {};

template <>
struct is_system_msg_handler<void(exit_msg)> : std::true_type {};

template <>
struct is_system_msg_handler<void(down_msg)> : std::true_type {};

// Tests whether the input list (IList) matches the
// signature list (SList) for a typed actor behavior
template <class SList, class IList>
struct valid_input {
  // strip exit_msg and down_msg from input types,
  // because they're always allowed
  using adjusted_slist = typename tl_filter_not<SList,
                                                is_system_msg_handler>::type;
  using adjusted_ilist = typename tl_filter_not<IList,
                                                is_system_msg_handler>::type;
  // check for each element in IList that there's an element in SList that
  // (1) has an identical input type list
  // (2)  has an identical output type list
  //   OR the output of the element in IList is skip_t
  static_assert(detail::tl_is_distinct<IList>::value,
                "given pattern is not distinct");
  static constexpr bool value = tl_size<adjusted_slist>::value
                                  == tl_size<adjusted_ilist>::value
                                && tl_forall<
                                  adjusted_ilist,
                                  valid_input_predicate<
                                    adjusted_slist>::template inner>::value;
};

// this function is called from typed_behavior<...>::set and its whole
// purpose is to give users a nicer error message on a type mismatch
// (this function only has the type information needed to understand the error)
template <class SignatureList, class InputList>
void static_check_typed_behavior_input() {
  constexpr bool is_valid = valid_input<SignatureList, InputList>::value;
  // note: it might be worth considering to allow a wildcard in the
  //     InputList if its return type is identical to all "missing"
  //     input types ... however, it might lead to unexpected results
  //     and would cause a lot of not-so-straightforward code here
  static_assert(is_valid, "given pattern cannot be used to initialize "
                          "typed behavior (exact match needed)");
}

} // namespace caf::detail

namespace caf::mixin {

template <class, class, class>
class behavior_stack_based_impl;

} // namespace caf::mixin

namespace caf {

/// Tag type for constructing a `typed_behavior` with an incomplete list of
/// message handlers, delegating to the default handler for all unmatched
/// inputs.
struct partial_behavior_init_t {};

constexpr partial_behavior_init_t partial_behavior_init
  = partial_behavior_init_t{};

template <class... Sigs>
class typed_behavior {
public:
  // -- friends ----------------------------------------------------------------

  template <class... OtherSigs>
  friend class typed_actor;

  template <class... OtherSigs>
  friend class typed_behavior;

  template <class, class, class>
  friend class mixin::behavior_stack_based_impl;

  // -- member types -----------------------------------------------------------

  /// Stores the template parameter pack in a type list.
  using signatures = detail::type_list<Sigs...>;

  // -- constructors, destructors, and assignment operators --------------------

  typed_behavior(typed_behavior&&) = default;
  typed_behavior(const typed_behavior&) = default;
  typed_behavior& operator=(typed_behavior&&) = default;
  typed_behavior& operator=(const typed_behavior&) = default;

  template <class... Ts>
  typed_behavior(const typed_behavior<Ts...>& other) : bhvr_(other.bhvr_) {
    using other_signatures = detail::type_list<Ts...>;
    using m = interface_mismatch_t<other_signatures, signatures>;
    // trigger static assert on mismatch
    detail::static_error_printer<static_cast<int>(sizeof...(Ts)), m::value,
                                 typename m::xs, typename m::ys>
      guard;
    CAF_IGNORE_UNUSED(guard);
  }

  template <class T, class... Ts>
  typed_behavior(T x, Ts... xs) {
    set(detail::make_behavior(std::move(x), std::move(xs)...));
  }

  template <class... Ts>
  typed_behavior(partial_behavior_init_t, Ts... xs)
    : typed_behavior(unsafe_behavior_init, behavior{std::move(xs)...}) {
    // TODO: implement type checking.
  }

  typed_behavior(unsafe_behavior_init_t, behavior x) : bhvr_(std::move(x)) {
    // nop
  }

  typed_behavior(unsafe_behavior_init_t, message_handler x)
    : bhvr_(std::move(x)) {
    // nop
  }

  // -- modifiers --------------------------------------------------------------

  /// Exchanges the contents of this and other.
  void swap(typed_behavior& other) {
    bhvr_.swap(other.bhvr_);
  }

  /// Invokes the timeout callback.
  void handle_timeout() {
    bhvr_.handle_timeout();
  }

  // -- observers --------------------------------------------------------------

  /// Returns whether this behavior contains any callbacks.
  explicit operator bool() const {
    return static_cast<bool>(bhvr_);
  }

  /// Returns the duration after which receives using
  /// this behavior should time out.
  timespan timeout() const noexcept {
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

  template <class... Ts, class TimeoutDefinition>
  void set(intrusive_ptr<
           detail::default_behavior_impl<std::tuple<Ts...>, TimeoutDefinition>>
             bp) {
    using found_signatures = detail::type_list<deduce_mpi_t<Ts>...>;
    using m = interface_mismatch_t<found_signatures, signatures>;
    // trigger static assert on mismatch
    detail::static_error_printer<static_cast<int>(sizeof...(Ts)), m::value,
                                 typename m::xs, typename m::ys>
      guard;
    CAF_IGNORE_UNUSED(guard);
    // final (type-erasure) step
    intrusive_ptr<detail::behavior_impl> ptr = std::move(bp);
    bhvr_.assign(std::move(ptr));
  }

  behavior bhvr_;
};

template <class T>
struct is_typed_behavior : std::false_type {};

template <class... Sigs>
struct is_typed_behavior<typed_behavior<Sigs...>> : std::true_type {};

/// Creates a typed behavior from given function objects.
template <class... Fs>
typed_behavior<deduce_mpi_t<Fs>...> make_typed_behavior(Fs... fs) {
  return {std::move(fs)...};
}

} // namespace caf
