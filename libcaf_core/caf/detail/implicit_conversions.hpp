// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/type_id.hpp"

#include <string>
#include <type_traits>

namespace caf::detail {

template <class T,
          bool IsDyn = std::is_base_of_v<dynamically_typed_actor_base, T>,
          bool IsStat = std::is_base_of_v<statically_typed_actor_base, T>>
struct implicit_actor_conversions {
  using type = T;
};

template <class T>
struct implicit_actor_conversions<T, true, false> {
  using type = actor;
};

template <class T>
struct implicit_actor_conversions<T, false, true> {
  using type = detail::tl_apply_t<typename T::signatures, typed_actor>;
};

template <>
struct implicit_actor_conversions<actor_control_block, false, false> {
  using type = strong_actor_ptr;
};

template <class T>
struct implicit_conversions {
  using type = std::conditional_t<std::is_convertible_v<T, error>, error,
                                  squash_if_int_t<T>>;
};

template <class T>
struct implicit_conversions<T*> : implicit_actor_conversions<T> {};

template <>
struct implicit_conversions<char*> {
  using type = std::string;
};

template <>
struct implicit_conversions<std::string_view> {
  using type = std::string;
};

template <size_t N>
struct implicit_conversions<char[N]> : implicit_conversions<char*> {};

template <>
struct implicit_conversions<const char*> : implicit_conversions<char*> {};

template <size_t N>
struct implicit_conversions<const char[N]> : implicit_conversions<char*> {};

template <>
struct implicit_conversions<char16_t*> {
  using type = std::u16string;
};

template <size_t N>
struct implicit_conversions<char16_t[N]> : implicit_conversions<char16_t*> {};

template <>
struct implicit_conversions<const char16_t*> : implicit_conversions<char16_t*> {
};

template <size_t N>
struct implicit_conversions<const char16_t[N]>
  : implicit_conversions<char16_t*> {};

template <>
struct implicit_conversions<char32_t*> {
  using type = std::u16string;
};

template <size_t N>
struct implicit_conversions<char32_t[N]> : implicit_conversions<char32_t*> {};

template <>
struct implicit_conversions<const char32_t*> : implicit_conversions<char32_t*> {
};

template <size_t N>
struct implicit_conversions<const char32_t[N]>
  : implicit_conversions<char32_t*> {};

template <>
struct implicit_conversions<scoped_actor> {
  using type = actor;
};

template <class T>
using implicit_conversions_t = typename implicit_conversions<T>::type;

template <class T>
struct strip_and_convert {
  using type
    = implicit_conversions_t<std::remove_const_t<std::remove_reference_t<T>>>;
};

template <class T>
using strip_and_convert_t = typename strip_and_convert<T>::type;

template <class T>
constexpr bool sendable = is_complete<type_id<strip_and_convert_t<T>>>;

} // namespace caf::detail
