/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <cstddef>
#include <limits>

#include "caf/net/socket_id.hpp"

namespace caf {
namespace net {

template <class Derived>
struct abstract_socket {
  socket_id id;

  constexpr abstract_socket(socket_id id) : id(id) {
    // nop
  }

  constexpr abstract_socket(const Derived& other) : id(other.id) {
    // nop
  }

  abstract_socket& operator=(const Derived& other) {
    id = other.id;
    return *this;
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, Derived& x) {
    return f(x.id);
  }

  friend constexpr bool operator==(Derived x, Derived y) {
    return x.id == y.id;
  }

  friend constexpr bool operator!=(Derived x, Derived y) {
    return x.id != y.id;
  }

  friend constexpr bool operator<(Derived x, Derived y) {
    return x.id < y.id;
  }
};

} // namespace net
} // namespace caf
