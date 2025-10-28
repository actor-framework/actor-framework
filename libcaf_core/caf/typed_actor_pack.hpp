// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

template <class... Ts>
class result;

template <class... Ts>
struct type_list;

} // namespace caf

namespace caf::detail {

template <class T>
struct is_message_handler_signature_oracle {
  static constexpr bool value = false;
};

template <class... Results, class... Args>
struct is_message_handler_signature_oracle<result<Results...>(Args...)> {
  static constexpr bool value = true;
};

} // namespace caf::detail

namespace caf {

template <class T>
concept message_handler_signature
  = detail::is_message_handler_signature_oracle<T>::value;

} // namespace caf

namespace caf::detail {

template <class T>
struct typed_actor_trait_checker;

template <class... Ts>
struct typed_actor_trait_checker<type_list<Ts...>> {
  // The only use case for this template is to be used in the
  // `message_handler_signature` concept. If this template is instantiated,
  // we want to "deep check" the signatures as well to raise compile-time errors
  // early in case the trait contains any invalid signatures. If we would simply
  // assign the result of that to `value` instead of using `static_assert`,
  // the user would not see which signature is invalid.
  static_assert((message_handler_signature<Ts> && ...));
  static constexpr bool value = true;
};

} // namespace caf::detail

namespace caf {

/// Checks whether `Ts...` is a valid template parameter pack for `typed_actor`.
template <class... Ts>
concept typed_actor_pack =
  // Either we have exactly one trait argument.
  (sizeof...(Ts) == 1
   && (detail::typed_actor_trait_checker<typename Ts::signatures>::value
       && ...))
  // Or we have at least one message handler signature.
  || (sizeof...(Ts) > 0 && (message_handler_signature<Ts> && ...));

} // namespace caf
