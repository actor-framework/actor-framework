// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>

#include "caf/deep_to_string.hpp"

namespace caf::detail {

/// Enables automagical string conversion for `CAF_ARG`.
template <class T>
struct single_arg_wrapper {
  const char* name;
  const T& value;
  single_arg_wrapper(const char* x, const T& y) : name(x), value(y) {
    // nop
  }
};

template <class T>
std::string to_string(const single_arg_wrapper<T>& x) {
  std::string result = x.name;
  result += " = ";
  result += deep_to_string(x.value);
  return result;
}

template <class Iterator>
struct range_arg_wrapper {
  const char* name;
  Iterator first;
  Iterator last;
  range_arg_wrapper(const char* x, Iterator begin, Iterator end)
    : name(x), first(begin), last(end) {
    // nop
  }
};

template <class Iterator>
std::string to_string(const range_arg_wrapper<Iterator>& x) {
  std::string result = x.name;
  result += " = ";
  struct dummy {
    Iterator first;
    Iterator last;
    Iterator begin() const {
      return first;
    }
    Iterator end() const {
      return last;
    }
  };
  dummy y{x.first, x.last};
  result += deep_to_string(y);
  return result;
}

/// Used to implement `CAF_ARG`.
template <class T>
single_arg_wrapper<T> make_arg_wrapper(const char* name, const T& value) {
  return {name, value};
}

/// Used to implement `CAF_ARG`.
template <class Iterator>
range_arg_wrapper<Iterator>
make_arg_wrapper(const char* name, Iterator first, Iterator last) {
  return {name, first, last};
}

} // namespace caf::detail
