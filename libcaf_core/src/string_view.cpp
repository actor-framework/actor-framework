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

#include "caf/string_view.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "caf/config.hpp"
#include "caf/raise_error.hpp"

namespace {

using size_type = caf::string_view::size_type;

} // namespace anonymous

namespace caf {

// -- iterator access ----------------------------------------------------------

string_view::const_reverse_iterator string_view::rbegin() const noexcept {
  return reverse_iterator{end()};
}

string_view::const_reverse_iterator string_view::rend() const noexcept {
  return reverse_iterator{begin()};
}

string_view::const_reverse_iterator string_view::crbegin() const noexcept {
  return rbegin();
}

string_view::const_reverse_iterator string_view::crend() const noexcept {
  return rend();
}

// -- element access -----------------------------------------------------------

string_view::const_reference string_view::at(size_type pos) const {
  if (pos < size_)
    return data_[pos];
  CAF_RAISE_ERROR(std::out_of_range, "string_view::at out of range");
}

// -- modifiers ----------------------------------------------------------------

void string_view::remove_prefix(size_type n) {
  if (n < size()) {
    data_ += n;
    size_ -= n;
  } else {
    size_ = 0;
  }
}

void string_view::remove_suffix(size_type n) {
  if (n < size())
    size_ -= n;
  else
    size_ = 0;
}

void string_view::assign(const_pointer data, size_type len) {
  data_ = data;
  size_ = len;
}

// -- algortihms ---------------------------------------------------------------

string_view::size_type string_view::copy(pointer dest, size_type n,
                                         size_type pos) const {
  if (pos > size_)
    CAF_RAISE_ERROR("string_view::copy out of range");
  auto first = begin() + pos;
  auto end = first + std::min(n, size() - pos);
  auto cpy_end = std::copy(first, end, dest);
  return static_cast<size_type>(std::distance(dest, cpy_end));
}

string_view string_view::substr(size_type pos, size_type n) const noexcept {
  if (pos >= size())
    return {};
  return {data_ + pos, std::min(size_ - pos, n)};
}

int string_view::compare(string_view str) const noexcept {
  auto s0 = size();
  auto s1 = str.size();
  auto fallback = [](int x, int y) {
    return x == 0 ? y : x;
  };
  if (s0 == s1)
    return strncmp(data(), str.data(), s0);
  else if (s0 < s1)
    return fallback(strncmp(data(), str.data(), s0), -1);
  return fallback(strncmp(data(), str.data(), s1), 1);
}

int string_view::compare(size_type pos1, size_type n1,
                         string_view str) const noexcept {
  return substr(pos1, n1).compare(str);
}

int string_view::compare(size_type pos1, size_type n1, string_view str,
                         size_type pos2, size_type n2) const noexcept {
  return substr(pos1, n1).compare(str.substr(pos2, n2));
}

int string_view::compare(const_pointer str) const noexcept {
  return strncmp(data(), str, size());
}

int string_view::compare(size_type pos, size_type n,
                         const_pointer str) const noexcept {
  return substr(pos, n).compare(str);
}

int string_view::compare(size_type pos1, size_type n1,
                         const_pointer str, size_type n2) const noexcept {
  return substr(pos1, n1).compare(string_view{str, n2});
}

size_type string_view::find(string_view str, size_type pos) const noexcept {
  string_view tmp;
  if (pos < size_)
    tmp.assign(data_ + pos, size_ - pos);
  auto first = tmp.begin();
  auto last = tmp.end();
  auto i = std::search(first, last, str.begin(), str.end());
  if (i != last)
    return static_cast<size_type>(std::distance(first, i)) + pos;
  return npos;
}

size_type string_view::find(value_type ch, size_type pos) const noexcept {
  return find(string_view{&ch, 1}, pos);
}

size_type string_view::find(const_pointer str, size_type pos,
                            size_type n) const noexcept {
  return find(string_view{str, n}, pos);
}

size_type string_view::find(const_pointer str, size_type pos) const noexcept {
  return find(string_view{str, strlen(str)}, pos);
}

size_type string_view::rfind(string_view str, size_type pos) const noexcept {
  if (size() < str.size())
    return npos;
  if (str.empty())
    return std::min(size(), pos);
  string_view tmp{data_, std::min(size() - str.size(), pos) + str.size()};
  auto first = tmp.begin();
  auto last = tmp.end();
  auto i = std::find_end(first, last, str.begin(), str.end());
  return i != last ? static_cast<size_type>(std::distance(first, i)) : npos;
}

size_type string_view::rfind(value_type ch, size_type pos) const noexcept {
  return rfind(string_view{&ch, 1}, pos);
}

size_type string_view::rfind(const_pointer str, size_type pos,
                             size_type n) const noexcept {
  return rfind(string_view{str, n}, pos);
}

size_type string_view::rfind(const_pointer str, size_type pos) const noexcept {
  return rfind(string_view{str, strlen(str)}, pos);
}

size_type string_view::find_first_of(string_view str,
                                     size_type pos) const noexcept {
  if (empty() || str.empty())
    return npos;
  if (pos >= size())
    return npos;
  if (str.size() == 1)
    return find(str.front(), pos);
  string_view tmp{data_ + pos, size_ - pos};
  auto first = tmp.begin();
  auto last = tmp.end();
  auto i = std::find_first_of(first, last, str.begin(), str.end());
  return i != last ? static_cast<size_type>(std::distance(first, i)) + pos
                   : npos;
}

size_type string_view::find_first_of(value_type ch,
                                     size_type pos) const noexcept {
  return find(ch, pos);
}

size_type string_view::find_first_of(const_pointer str, size_type pos,
                                     size_type n) const noexcept {
  return find_first_of(string_view{str, n}, pos);
}

size_type string_view::find_first_of(const_pointer str,
                                     size_type pos) const noexcept {
  return find_first_of(string_view{str, strlen(str)}, pos);
}

size_type string_view::find_last_of(string_view str,
                                    size_type pos) const noexcept {
  string_view tmp{data_, pos < size_ ? pos + 1 : size_};
  auto result = tmp.find_first_of(str);
  if (result == npos)
    return npos;
  for (;;) {
    auto next = tmp.find_first_of(str, result + 1);
    if (next == npos)
      return result;
    result = next;
  }
}

size_type string_view::find_last_of(value_type ch,
                                    size_type pos) const noexcept {
  return rfind(ch, pos);
}

size_type string_view::find_last_of(const_pointer str, size_type pos,
                                    size_type n) const noexcept {
  return find_last_of(string_view{str, n}, pos);
}

size_type string_view::find_last_of(const_pointer str,
                                    size_type pos) const noexcept {
  return find_last_of(string_view{str, strlen(str)}, pos);
}

size_type string_view::find_first_not_of(string_view str,
                                         size_type pos) const noexcept {
  if (str.size() == 1)
    return find_first_not_of(str.front(), pos);
  if (pos >= size())
    return npos;
  auto pred = [&](value_type x) { return str.find(x) == npos; };
  string_view tmp{data_ + pos, size_ - pos};
  auto first = tmp.begin();
  auto last = tmp.end();
  auto i = std::find_if(first, last, pred);
  return i != last ? static_cast<size_type>(std::distance(first, i)) + pos
                   : npos;
}

size_type string_view::find_first_not_of(value_type ch,
                                         size_type pos) const noexcept {
  if (pos >= size())
    return npos;
  auto pred = [=](value_type x) { return x != ch; };
  string_view tmp{data_ + pos, size_ - pos};
  auto first = tmp.begin();
  auto last = tmp.end();
  auto i = std::find_if(first, last, pred);
  return i != last ? static_cast<size_type>(std::distance(first, i)) + pos
                   : npos;
}

size_type string_view::find_first_not_of(const_pointer str, size_type pos,
                                         size_type n) const noexcept {
  return find_first_not_of(string_view{str, n}, pos);
}

size_type string_view::find_first_not_of(const_pointer str,
                                         size_type pos) const noexcept {
  return find_first_not_of(string_view{str, strlen(str)}, pos);
}

size_type string_view::find_last_not_of(string_view str,
                                        size_type pos) const noexcept {
  string_view tmp{data_, pos < size_ ? pos + 1 : size_};
  auto result = tmp.find_first_not_of(str);
  if (result == npos)
    return npos;
  for (;;) {
    auto next = tmp.find_first_not_of(str, result + 1);
    if (next == npos)
      return result;
    result = next;
  }
}

size_type string_view::find_last_not_of(value_type ch,
                                        size_type pos) const noexcept {
  return find_last_not_of(string_view{&ch, 1}, pos);
}

size_type string_view::find_last_not_of(const_pointer str, size_type pos,
                                        size_type n) const noexcept {
  return find_last_not_of(string_view{str, n}, pos);
}

size_type string_view::find_last_not_of(const_pointer str,
                                        size_type pos) const noexcept {
  return find_last_not_of(string_view{str, strlen(str)}, pos);
}

} // namespace caf

namespace std {

std::ostream& operator<<(std::ostream& out, caf::string_view str) {
  for (auto ch : str)
    out.put(ch);
  return out;
}

} // namespace std
