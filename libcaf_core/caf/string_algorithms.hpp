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

#include <cmath>     // fabs
#include <string>
#include <vector>
#include <limits>
#include <sstream>
#include <algorithm>
#include <type_traits>

#include "caf/config.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

// provide boost::split compatible interface

inline std::string is_any_of(std::string arg) {
  return arg;
}

constexpr bool token_compress_on = false;

template <class Container, class Str, class Delim>
void split(Container& result, const Str& str, const Delim& delims,
           bool keep_all = true) {
  size_t pos = 0;
  size_t prev = 0;
  while ((pos = str.find_first_of(delims, prev)) != std::string::npos) {
    if (pos > prev) {
      auto substr = str.substr(prev, pos - prev);
      if (!substr.empty() || keep_all) {
        result.push_back(std::move(substr));
      }
    }
    prev = pos + 1;
  }
  if (prev < str.size())
    result.push_back(str.substr(prev, std::string::npos));
}

template <class Iterator>
class iterator_range {
public:
  using iterator = Iterator;

  iterator_range(iterator first, iterator last) : begin_(first), end_(last) {
    // nop
  }

  iterator begin() const {
    return begin_;
  }

  iterator end() const {
    return end_;
  }

private:
  iterator begin_;
  iterator end_;
};


template <class Container>
std::string join(const Container& c, const std::string& glue) {
  auto begin = c.begin();
  auto end = c.end();
  bool first = true;
  std::ostringstream oss;
  for ( ; begin != end; ++begin) {
    if (first)
      first = false;
    else
      oss << glue;
    oss << *begin;
  }
  return oss.str();
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

template<size_t S>
bool starts_with(const std::string& str, const char (&prefix)[S]) {
  return str.compare(0, S - 1, prefix) == 0;
}

template <size_t S>
bool ends_with(const std::string& str, const char (&suffix)[S]) {
  auto n = str.size();
  auto m = S - 1;
  if (n >= m)
    return str.compare(n - m, m, suffix) == 0;
  return false;
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
  return value;
}

} // namespace caf

