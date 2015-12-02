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
    typename std::conditional<
      std::is_convertible<T, std::string>::value,
      std::string,
      T
    >::type;
  // convert C strings to std::u16string if possible
  using step2 =
    typename std::conditional<
      std::is_convertible<step1, std::u16string>::value,
      std::u16string,
      step1
    >::type;
  // convert C strings to std::u32string if possible
  using step3 =
    typename std::conditional<
      std::is_convertible<step2, std::u32string>::value,
      std::u32string,
      step2
    >::type;
  using step4 =
    typename std::conditional<
      std::is_convertible<step3, abstract_actor*>::value
      || std::is_same<scoped_actor, step3>::value,
      actor,
      step3
    >::type;
  using type =
    typename std::conditional<
      std::is_convertible<step4, error>::value,
      error,
      step4
    >::type;
};

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

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_IMPLICIT_CONVERSIONS_HPP
