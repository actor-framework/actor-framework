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
#include <sstream>
#include <type_traits>

#include "caf/message.hpp"
#include "caf/allowed_unsafe_message_type.hpp"

#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

/// Unboxes atom constants, i.e., converts `atom_constant<V>` to `V`.
/// @relates message
template <class T, int IsPlaceholderRes = std::is_placeholder<T>::value>
struct unbox_message_element {
  using type = index_mapping;
};

template <class T>
struct unbox_message_element<T, 0> {
  using type = T;
};

template <atom_value V>
struct unbox_message_element<atom_constant<V>, 0> {
  using type = atom_value;
};

template <>
struct unbox_message_element<actor_control_block*, 0> {
  using type = strong_actor_ptr;
};

///
template <class T>
struct is_serializable_or_whitelisted {
  static constexpr bool value = detail::is_serializable<T>::value
                                || allowed_unsafe_message_type<T>::value;
};

/// Returns a new `message` containing the values `(x, xs...)`.
/// @relates message
template <class T, class... Ts>
typename std::enable_if<
  !std::is_same<message, typename std::decay<T>::type>::value
  || (sizeof...(Ts) > 0),
  message
>::type
make_message(T&& x, Ts&&... xs) {
  using namespace caf::detail;
  using stored_types =
    type_list<
      typename unbox_message_element<
        typename strip_and_convert<T>::type
      >::type,
      typename unbox_message_element<
        typename strip_and_convert<Ts>::type
      >::type...
    >;
  static_assert(tl_forall<stored_types, is_serializable_or_whitelisted>::value,
                "at least one type is neither inspectable via "
                "inspect(Inspector&, T&) nor serializable via "
                "'serialize(Processor&, T&, const unsigned int)' or "
                "`T::serialize(Processor&, const unsigned int)`; "
                "you can whitelist individual types by "
                "specializing `caf::allowed_unsafe_message_type<T>` "
                "or using the macro CAF_ALLOW_UNSAFE_MESSAGE_TYPE");
  using storage = typename tl_apply<stored_types, tuple_vals>::type;
  auto ptr = make_counted<storage>(std::forward<T>(x), std::forward<Ts>(xs)...);
  return message{detail::message_data::cow_ptr{std::move(ptr)}};
}

/// Returns a copy of @p other.
/// @relates message
inline message make_message(message other) {
  return other;
}

/// Returns an empty `message`.
/// @relates message
inline message make_message() {
  return message{};
}

struct message_factory {
  template <class... Ts>
  message operator()(Ts&&... xs) const {
    return make_message(std::forward<Ts>(xs)...);
  }
};

/// Converts the tuple `xs` to a message.
template <class... Ts>
message make_message_from_tuple(std::tuple<Ts...> xs) {
  message_factory f;
  return detail::apply_moved_args(f, detail::get_indices(xs), xs);
}

} // namespace caf

