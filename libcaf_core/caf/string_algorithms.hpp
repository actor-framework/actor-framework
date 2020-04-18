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

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/string_view.hpp"

namespace caf {

// provide boost::split compatible interface

constexpr string_view is_any_of(string_view arg) noexcept {
  return arg;
}

constexpr bool token_compress_on = false;

CAF_CORE_EXPORT void split(std::vector<std::string>& result, string_view str,
                           string_view delims, bool keep_all = true);

CAF_CORE_EXPORT void split(std::vector<string_view>& result, string_view str,
                           string_view delims, bool keep_all = true);

CAF_CORE_EXPORT void split(std::vector<std::string>& result, string_view str,
                           char delim, bool keep_all = true);

CAF_CORE_EXPORT void split(std::vector<string_view>& result, string_view str,
                           char delim, bool keep_all = true);

template <class InputIterator>
std::string join(InputIterator first, InputIterator last, string_view glue) {
  if (first == last)
    return {};
  std::ostringstream oss;
  oss << *first++;
  for (; first != last; ++first)
    oss << glue << *first;
  return oss.str();
}

template <class Container>
std::string join(const Container& c, string_view glue) {
  return join(c.begin(), c.end(), glue);
}

/// Replaces all occurrences of `what` by `with` in `str`.
CAF_CORE_EXPORT void
replace_all(std::string& str, string_view what, string_view with);

/// Returns whether `str` begins with `prefix`.
CAF_CORE_EXPORT bool starts_with(string_view str, string_view prefix);

/// Returns whether `str` ends with `suffix`.
CAF_CORE_EXPORT bool ends_with(string_view str, string_view suffix);

} // namespace caf
