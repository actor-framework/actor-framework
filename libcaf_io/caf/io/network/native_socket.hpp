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

#include <cstdint>

#include "caf/config.hpp"

namespace caf {
namespace io {
namespace network {

#ifdef CAF_WINDOWS
  using native_socket = size_t;
  constexpr native_socket invalid_native_socket = static_cast<native_socket>(-1);
  inline int64_t int64_from_native_socket(native_socket sock) {
    return sock == invalid_native_socket ? -1 : static_cast<int64_t>(sock);
  }
#else
  using native_socket = int;
  constexpr native_socket invalid_native_socket = -1;
  inline int64_t int64_from_native_socket(native_socket sock) {
    return static_cast<int64_t>(sock);
  }
#endif

} // namespace network
} // namespace io
} // namespace caf

