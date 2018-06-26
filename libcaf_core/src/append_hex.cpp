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

#include "caf/detail/stringification_inspector.hpp"

namespace caf {
namespace detail {

void append_hex(std::string& result, const uint8_t* xs, size_t n) {
  if (n == 0) {
    result += "00";
    return;
  }
  auto tbl = "0123456789ABCDEF";
  char buf[3] = {0, 0, 0};
  for (size_t i = 0; i < n; ++i) {
    auto c = xs[i];
    buf[0] = tbl[c >> 4];
    buf[1] = tbl[c & 0x0F];
    result += buf;
  }
}

} // namespace detail
} // namespace caf
