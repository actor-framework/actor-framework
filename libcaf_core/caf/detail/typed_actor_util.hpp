// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/delegated.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/response_promise.hpp"
#include "caf/system_messages.hpp"
#include "caf/typed_response_promise.hpp"

#include <tuple>

namespace caf::detail {

template <class... Ts>
struct make_response_promise_helper {
  using type = typed_response_promise<Ts...>;
};

template <class... Ts>
struct make_response_promise_helper<typed_response_promise<Ts...>> {
  using type = typed_response_promise<Ts...>;
};

template <>
struct make_response_promise_helper<response_promise> {
  using type = response_promise;
};

/// Convenience alias for `make_response_promise_helper<Ts...>::type`.
template <class... Ts>
using make_response_promise_helper_t =
  typename make_response_promise_helper<Ts...>::type;

template <class... Ts>
using response_promise_t = make_response_promise_helper_t<Ts...>;

template <class Output, class F>
struct type_checker {
  static void check() {
    using arg_types = typename tl_map<typename get_callable_trait<F>::arg_types,
                                      std::decay>::type;
    static_assert(std::is_same_v<Output, arg_types>
                    || (std::is_same_v<Output, type_list<void>>
                        && std::is_same_v<arg_types, type_list<>>),
                  "wrong functor signature");
  }
};

template <class F>
struct type_checker<message, F> {
  static void check() {
    // nop
  }
};

/// Generates an error using static_assert on an interface mismatch.
/// @tparam NumMessageHandlers The number of message handlers
///                            provided by the user.
/// @tparam Pos The index at which an error was detected or a negative value
///             if too many or too few handlers were provided.
/// @tparam RemainingXs The remaining deduced messaging interfaces of the
///                     provided message handlers at the time of the error.
/// @tparam RemainingYs The remaining unimplemented message handler
///                     signatures at the time of the error.
template <int NumMessageHandlers, int Pos, class RemainingXs, class RemainingYs>
struct static_error_printer {
  static_assert(NumMessageHandlers == Pos,
                "unexpected handler some index > 20");
};

template <int N, class Xs, class Ys>
struct static_error_printer<N, -2, Xs, Ys> {
  static_assert(N == -1, "too many message handlers");
};

template <int N, class Xs, class Ys>
struct static_error_printer<N, -1, Xs, Ys> {
  static_assert(N == -1, "not enough message handlers");
};

#define CAF_STATICERR(x)                                                       \
  template <int N, class Xs, class Ys>                                         \
  struct static_error_printer<N, x, Xs, Ys> {                                  \
    static_assert(N == x, "unexpected handler at index " #x " (0-based)");     \
  }

CAF_STATICERR(0);
CAF_STATICERR(1);
CAF_STATICERR(2);
CAF_STATICERR(3);
CAF_STATICERR(4);
CAF_STATICERR(5);
CAF_STATICERR(6);
CAF_STATICERR(7);
CAF_STATICERR(8);
CAF_STATICERR(9);
CAF_STATICERR(10);
CAF_STATICERR(11);
CAF_STATICERR(12);
CAF_STATICERR(13);
CAF_STATICERR(14);
CAF_STATICERR(15);
CAF_STATICERR(16);
CAF_STATICERR(17);
CAF_STATICERR(18);
CAF_STATICERR(19);
CAF_STATICERR(20);

template <class... Ts>
struct extend_with_helper;

template <class... Xs>
struct extend_with_helper<typed_actor<Xs...>> {
  using type = typed_actor<Xs...>;
};

template <class... Xs, class... Ys, class... Ts>
struct extend_with_helper<typed_actor<Xs...>, typed_actor<Ys...>, Ts...>
  : extend_with_helper<typed_actor<Xs..., Ys...>, Ts...> {
  // nop
};

template <class F>
struct is_normalized_signature {
  static constexpr bool value = false;
};

template <class T>
inline constexpr bool is_decayed
  = !std::is_reference_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>;

template <class... Out, class... In>
struct is_normalized_signature<result<Out...>(In...)> {
  static constexpr bool value = (is_decayed<Out> && ...)
                                && (is_decayed<In> && ...);
};

template <class F>
constexpr bool is_normalized_signature_v = is_normalized_signature<F>::value;

} // namespace caf::detail
