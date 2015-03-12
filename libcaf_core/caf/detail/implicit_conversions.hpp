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
  // convert C strings to std::string if possible
  using step1 =
    typename replace_type<
      T, std::string,
      std::is_same<T, const char*>, std::is_same<T, char*>,
      std::is_same<T, char[]>, is_array_of<T, char>,
      is_array_of<T, const char>
    >::type;
  // convert C strings to std::u16string if possible
  using step2 =
    typename replace_type<
      step1, std::u16string,
      std::is_same<step1, const char16_t*>, std::is_same<step1, char16_t*>,
      is_array_of<step1, char16_t>
    >::type;
  // convert C strings to std::u32string if possible
  using step3 =
    typename replace_type<
      step2, std::u32string,
      std::is_same<step2, const char32_t*>, std::is_same<step2, char32_t*>,
      is_array_of<step2, char32_t>
    >::type;
  using type =
    typename replace_type<
      step3, actor,
      std::is_convertible<T, abstract_actor*>, std::is_same<scoped_actor, T>
    >::type;
};

template <class T>
struct strip_and_convert {
  using type =
    typename implicit_conversions<
      typename std::decay<T>::type
    >::type;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_IMPLICIT_CONVERSIONS_HPP
