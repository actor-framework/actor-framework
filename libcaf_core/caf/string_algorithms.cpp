// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/string_algorithms.hpp"

namespace caf {

namespace {

template <class F>
void split_impl(F consume, std::string_view str, std::string_view delims,
                bool keep_all) {
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
    consume(std::string_view{});
}

} // namespace

void split(std::vector<std::string>& result, std::string_view str,
           std::string_view delims, bool keep_all) {
  auto f = [&](std::string_view sv) {
    result.emplace_back(sv.begin(), sv.end());
  };
  return split_impl(f, str, delims, keep_all);
}

void split(std::vector<std::string_view>& result, std::string_view str,
           std::string_view delims, bool keep_all) {
  auto f = [&](std::string_view sv) { result.emplace_back(sv); };
  return split_impl(f, str, delims, keep_all);
}

void split(std::vector<std::string>& result, std::string_view str, char delim,
           bool keep_all) {
  split(result, str, std::string_view{&delim, 1}, keep_all);
}

void split(std::vector<std::string_view>& result, std::string_view str,
           char delim, bool keep_all) {
  split(result, str, std::string_view{&delim, 1}, keep_all);
}

std::string_view trim(std::string_view str) {
  auto non_whitespace = [](char c) { return !isspace(c); };
  if (std::any_of(str.begin(), str.end(), non_whitespace)) {
    while (str.front() == ' ')
      str.remove_prefix(1);
    while (str.back() == ' ')
      str.remove_suffix(1);
  } else {
    str = std::string_view{};
  }
  return str;
}

bool icase_equal(std::string_view x, std::string_view y) {
  auto cmp = [](const char lhs, const char rhs) {
    auto to_uchar = [](char c) { return static_cast<unsigned char>(c); };
    return tolower(to_uchar(lhs)) == tolower(to_uchar(rhs));
  };
  return std::equal(x.begin(), x.end(), y.begin(), y.end(), cmp);
}

std::pair<std::string_view, std::string_view> split_by(std::string_view str,
                                                       std::string_view sep) {
  // We need actual char* pointers for make_string_view. On some compilers,
  // begin() and end() may return iterator types, so we use data() instead.
  auto str_end = str.data() + str.size();
  auto i = std::search(str.data(), str_end, sep.begin(), sep.end());
  if (i != str_end) {
    return {make_string_view(str.data(), i),
            make_string_view(i + sep.size(), str_end)};
  } else {
    return {str, {}};
  }
}

void replace_all(std::string& str, std::string_view what,
                 std::string_view with) {
  // end(what) - 1 points to the null-terminator
  auto next = [&](std::string::iterator pos) -> std::string::iterator {
    return std::search(pos, str.end(), what.begin(), what.end());
  };
  auto i = next(str.begin());
  while (i != str.end()) {
    auto before = std::distance(str.begin(), i);
    str.replace(i, i + static_cast<ptrdiff_t>(what.size()), with.begin(),
                with.end());
    // Iterator i became invalidated -> use new iterator pointing to the first
    // character after the replaced text.
    i = next(str.begin() + before + static_cast<ptrdiff_t>(with.size()));
  }
}

bool starts_with(std::string_view str, std::string_view prefix) {
  return str.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(std::string_view str, std::string_view suffix) {
  auto n = str.size();
  auto m = suffix.size();
  return n >= m ? str.compare(n - m, m, suffix) == 0 : false;
}

} // namespace caf
