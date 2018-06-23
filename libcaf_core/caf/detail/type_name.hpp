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

#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "caf/fwd.hpp"
#include "caf/timestamp.hpp"

namespace caf {
namespace detail {

template <size_t Bytes>
struct type_name_builder_int_size;

template <>
struct type_name_builder_int_size<1> {
  inline void operator()(std::string& result) const {
    result += "8";
  }
};

template <>
struct type_name_builder_int_size<2> {
  inline void operator()(std::string& result) const {
    result += "16";
  }
};

template <>
struct type_name_builder_int_size<4> {
  inline void operator()(std::string& result) const {
    result += "32";
  }
};

template <>
struct type_name_builder_int_size<8> {
  inline void operator()(std::string& result) const {
    result += "64";
  }
};

template <class T, bool IsInteger = std::is_integral<T>::value>
struct type_name_builder;

template <>
struct type_name_builder<bool, true> {
  inline void operator()(std::string& result) const {
    result += "boolean";
  }
};

template <>
struct type_name_builder<float, false> {
  inline void operator()(std::string& result) const {
    result += "32-bit real";
  }
};

template <>
struct type_name_builder<double, false> {
  inline void operator()(std::string& result) const {
    result += "64-bit real";
  }
};

template <>
struct type_name_builder<timespan, false> {
  inline void operator()(std::string& result) const {
    result += "timespan";
  }
};

template <>
struct type_name_builder<std::string, false> {
  inline void operator()(std::string& result) const {
    result += "string";
  }
};

template <>
struct type_name_builder<atom_value, false> {
  inline void operator()(std::string& result) const {
    result += "atom";
  }
};

template <class T>
struct type_name_builder<T, true> {
  void operator()(std::string& result) const {
    // TODO: replace with if constexpr when switching to C++17
    if (!std::is_signed<T>::value)
      result += 'u';
    result += "int";
    type_name_builder_int_size<sizeof(T)> g;
    g(result);
  }
};

template <class T>
struct type_name_builder<std::vector<T>, false> {
  void operator()(std::string& result) const {
    result += "list of ";
    type_name_builder<T> g;
    g(result);
  }
};

template <class T>
struct type_name_builder<std::map<std::string, T>, false> {
  void operator()(std::string& result) const {
    result += "dictionary of ";
    type_name_builder<T> g;
    g(result);
  }
};

template <class T>
std::string type_name() {
  std::string result;
  type_name_builder<T> f;
  f(result);
  return result;
}

} // namespace detail
} // namespace caf

