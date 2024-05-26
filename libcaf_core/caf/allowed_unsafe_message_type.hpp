// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

// This file is referenced in the manual, do not modify without updating refs!
// ConfiguringActorApplications: 50-54

#pragma once

#include <type_traits>

namespace caf {

/// Template specializations can whitelist individual
/// types for unsafe message passing operations.
template <class T>
struct allowed_unsafe_message_type : std::false_type {};

template <class T>
struct is_allowed_unsafe_message_type : allowed_unsafe_message_type<T> {};

template <class T>
struct is_allowed_unsafe_message_type<T&> : allowed_unsafe_message_type<T> {};

template <class T>
struct is_allowed_unsafe_message_type<T&&> : allowed_unsafe_message_type<T> {};

template <class T>
struct is_allowed_unsafe_message_type<const T&>
  : allowed_unsafe_message_type<T> {};

template <class T>
constexpr bool is_allowed_unsafe_message_type_v
  = allowed_unsafe_message_type<T>::value;

} // namespace caf

#define CAF_ALLOW_UNSAFE_MESSAGE_TYPE(type_name)                               \
  namespace caf {                                                              \
  template <>                                                                  \
  struct allowed_unsafe_message_type<type_name> : std::true_type {};           \
  }
