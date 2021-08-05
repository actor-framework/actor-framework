// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

// provide boost::split compatible interface

constexpr std::string_view is_any_of(std::string_view arg) noexcept {
  return arg;
}

constexpr bool token_compress_on = false;

CAF_CORE_EXPORT void split(std::vector<std::string>& result,
                           std::string_view str, std::string_view delims,
                           bool keep_all = true);

CAF_CORE_EXPORT void split(std::vector<std::string_view>& result,
                           std::string_view str, std::string_view delims,
                           bool keep_all = true);

CAF_CORE_EXPORT void split(std::vector<std::string>& result,
                           std::string_view str, char delim,
                           bool keep_all = true);

CAF_CORE_EXPORT void split(std::vector<std::string_view>& result,
                           std::string_view str, char delim,
                           bool keep_all = true);

template <class InputIterator>
std::string
join(InputIterator first, InputIterator last, std::string_view glue) {
  if (first == last)
    return {};
  std::ostringstream oss;
  oss << *first++;
  for (; first != last; ++first)
    oss << glue << *first;
  return oss.str();
}

template <class Container>
std::string join(const Container& c, std::string_view glue) {
  return join(c.begin(), c.end(), glue);
}

/// Replaces all occurrences of `what` by `with` in `str`.
CAF_CORE_EXPORT void replace_all(std::string& str, std::string_view what,
                                 std::string_view with);

/// Returns whether `str` begins with `prefix`.
CAF_CORE_EXPORT bool starts_with(std::string_view str, std::string_view prefix);

/// Returns whether `str` ends with `suffix`.
CAF_CORE_EXPORT bool ends_with(std::string_view str, std::string_view suffix);

} // namespace caf
