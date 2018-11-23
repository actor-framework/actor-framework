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

#include "caf/string_algorithms.hpp"

namespace caf {

namespace {

template <class F>
void split_impl(F consume, string_view str, string_view delims, bool keep_all) {
  size_t pos = 0;
  size_t prev = 0;
  while ((pos = str.find_first_of(delims, prev)) != std::string::npos) {
    auto substr = str.substr(prev, pos - prev);
    if (keep_all || !substr.empty())
      consume(substr);
    prev = pos + 1;
  }
  if (prev < str.size())
    consume(str.substr(prev));
  else if (keep_all)
    consume(string_view{});
}

} // namespace <anonymous>

void split(std::vector<std::string>& result, string_view str,
           string_view delims, bool keep_all) {
  auto f = [&](string_view sv) {
    result.emplace_back(sv.begin(), sv.end());
  };
  return split_impl(f, str, delims, keep_all);
}

void split(std::vector<string_view>& result, string_view str,
           string_view delims, bool keep_all) {
  auto f = [&](string_view sv) {
    result.emplace_back(sv);
  };
  return split_impl(f, str, delims, keep_all);
}

void split(std::vector<std::string>& result, string_view str, char delim,
           bool keep_all) {
  split(result, str, string_view{&delim, 1}, keep_all);
}

void split(std::vector<string_view>& result, string_view str, char delim,
           bool keep_all) {
  split(result, str, string_view{&delim, 1}, keep_all);
}

void replace_all(std::string& str, string_view what, string_view with) {
  // end(what) - 1 points to the null-terminator
  auto next = [&](std::string::iterator pos) -> std::string::iterator {
    return std::search(pos, str.end(), what.begin(), what.end());
  };
  auto i = next(str.begin());
  while (i != str.end()) {
    auto before = std::distance(str.begin(), i);
    CAF_ASSERT(before >= 0);
    auto ws = static_cast<decltype(before)>(what.size());
    str.replace(i, i + ws, with.begin(), with.end());
    // Iterator i became invalidated -> use new iterator pointing to the first
    // character after the replaced text.
    i = next(str.begin() + before + ws);
  }
}

bool starts_with(string_view str, string_view prefix) {
  return str.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(string_view str, string_view suffix) {
  auto n = str.size();
  auto m = suffix.size();
  return n >= m ? str.compare(n - m, m, suffix) == 0 : false;
}

} // namespace caf
