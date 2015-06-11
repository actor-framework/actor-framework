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

#ifndef CAF_UTIL_ALGORITHM_HPP
#define CAF_UTIL_ALGORITHM_HPP

#include <cmath>     // fabs
#include <string>
#include <vector>
#include <limits>
#include <sstream>
#include <algorithm>
#include <type_traits>

#include "caf/detail/type_traits.hpp"

namespace caf {

// provide boost::split compatible interface

inline std::string is_any_of(std::string arg) {
  return arg;
}

constexpr bool token_compress_on = false;

void split(std::vector<std::string>& result,
       const std::string& str,
       const std::string& delimiters = " ",
       bool keep_empties = true);

template <class Iterator>
std::string join(Iterator begin, Iterator end, const std::string& glue) {
  bool first = true;
  std::ostringstream oss;
  for ( ; begin != end; ++begin) {
    if (first) first = false;
    else oss << glue;
    oss << *begin;
  }
  return oss.str();
}

template <class Container>
std::string join(const Container& c, const std::string& glue) {
  return join(c.begin(), c.end(), glue);
}

// end of recursion
inline void splice(std::string&, const std::string&) {
  // nop
}

template <class T, class... Ts>
void splice(std::string& str, const std::string& glue, T&& arg, Ts&&... xs) {
  str += glue;
  str += std::forward<T>(arg);
  splice(str, glue, std::forward<Ts>(xs)...);
}

template <ptrdiff_t WhatSize, ptrdiff_t WithSize>
void replace_all(std::string& str,
                 const char (&what)[WhatSize],
                 const char (&with)[WithSize]) {
  // end(what) - 1 points to the null-terminator
  auto next = [&](std::string::iterator pos) -> std::string::iterator {
    return std::search(pos, str.end(), std::begin(what), std::end(what) - 1);
  };
  auto i = next(std::begin(str));
  while (i != std::end(str)) {
    auto before = std::distance(std::begin(str), i);
    CAF_ASSERT(before >= 0);
    str.replace(i, i + WhatSize - 1, with);
    // i became invalidated -> use new iterator pointing
    // to the first character after the replaced text
    i = next(str.begin() + before + (WithSize - 1));
  }
}

/// Compares two values by using `operator==` unless two floating
/// point numbers are compared. In the latter case, the function
/// performs an epsilon comparison.
template <class T, typename U>
typename std::enable_if<
  ! std::is_floating_point<T>::value && ! std::is_floating_point<U>::value,
  bool
>::type
safe_equal(const T& lhs, const U& rhs) {
  return lhs == rhs;
}

template <class T, typename U>
typename std::enable_if<
  std::is_floating_point<T>::value || std::is_floating_point<U>::value,
  bool
>::type
safe_equal(const T& lhs, const U& rhs) {
  using res_type = decltype(lhs - rhs);
  return std::fabs(lhs - rhs) <= std::numeric_limits<res_type>::epsilon();
}

template <class T>
typename std::enable_if<
  std::is_arithmetic<T>::value,
  std::string
>::type
convert_to_str(T value) {
  return std::to_string(value);
}

inline std::string convert_to_str(std::string value) {
  return std::move(value);
}

// string projection
template <class T>
caf::optional<T> spro(const std::string& str) {
  T value;
  std::istringstream iss(str);
  if (iss >> value) {
    return value;
  }
  return caf::none;
}

} // namespace caf

#endif // CAF_UTIL_ALGORITHM_HPP
