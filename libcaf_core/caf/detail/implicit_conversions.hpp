/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_DETAIL_IMPLICIT_CONVERSIONS_HPP
#define CAF_DETAIL_IMPLICIT_CONVERSIONS_HPP

#include <string>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

template <class T>
struct implicit_conversions {
  using type =
    typename std::conditional<
      std::is_convertible<T, error>::value,
      error,
      T
    >::type;
};

template <class T>
struct implicit_conversions<T*> {
  using type =
    typename std::conditional<
      std::is_base_of<abstract_actor, T>::value,
      actor,
      T*
    >::type;
};

template <>
struct implicit_conversions<char*> {
  using type = std::string;
};

template <size_t N>
struct implicit_conversions<char[N]>
    : implicit_conversions<char*> {};

template <>
struct implicit_conversions<const char*>
    : implicit_conversions<char*> {};

template <size_t N>
struct implicit_conversions<const char[N]>
    : implicit_conversions<char*> {};

template <>
struct implicit_conversions<char16_t*> {
  using type = std::u16string;
};

template <size_t N>
struct implicit_conversions<char16_t[N]>
    : implicit_conversions<char16_t*> {};

template <>
struct implicit_conversions<const char16_t*>
    : implicit_conversions<char16_t*> {};

template <size_t N>
struct implicit_conversions<const char16_t[N]>
  : implicit_conversions<char16_t*> {};

template <>
struct implicit_conversions<char32_t*> {
  using type = std::u16string;
};

template <size_t N>
struct implicit_conversions<char32_t[N]>
    : implicit_conversions<char32_t*> {};

template <>
struct implicit_conversions<const char32_t*>
    : implicit_conversions<char32_t*> {};

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
  using type =
    typename implicit_conversions<
      typename std::remove_const<
        typename std::remove_reference<
          T
        >::type
      >::type
    >::type;
};

template <class T>
using strip_and_convert_t = typename strip_and_convert<T>::type;

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_IMPLICIT_CONVERSIONS_HPP
