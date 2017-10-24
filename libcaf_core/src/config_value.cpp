/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/config_value.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

namespace {

struct less_comparator {
  template <class T, class U>
  detail::enable_if_t<std::is_same<T, U>::value, bool>
  operator()(const T& x, const U& y) const {
    return x < y;
  }

  template <class T, class U>
  detail::enable_if_t<!std::is_same<T, U>::value, bool>
  operator()(const T&, const U&) const {
    // Sort by type index when comparing different types.
    using namespace detail;
    using types = config_value::types;
    return tl_index_of<types, T>::value < tl_index_of<types, U>::value;
  }
};

struct equal_comparator {
  template <class T, class U>
  detail::enable_if_t<std::is_same<T, U>::value, bool>
  operator()(const T& x, const U& y) const {
    return x == y;
  }

  template <class T, class U>
  detail::enable_if_t<!std::is_same<T, U>::value, bool>
  operator()(const T&, const U&) const {
    return false;
  }
};

} // namespace <anonymous>

config_value::~config_value() {
  // nop
}

void config_value::convert_to_list() {
  if (holds_alternative<T6>(data_))
    return;
  config_value tmp{std::move(*this)};
  set(T6{std::move(tmp)});
}

void config_value::append(config_value x) {
  convert_to_list();
  auto& xs = caf::get<T6>(data_);
  xs.emplace_back(std::move(x));
}

bool operator<(const config_value& x, const config_value& y) {
  less_comparator cmp;
  return visit(cmp, x.data(), y.data());
}

bool operator==(const config_value& x, const config_value& y) {
  equal_comparator cmp;
  return visit(cmp, x.data(), y.data());
}

} // namespace caf

