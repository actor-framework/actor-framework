/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/print.hpp"

#include "caf/config.hpp"

namespace caf::detail {

size_t print_timestamp(char* buf, size_t buf_size, time_t ts, size_t ms) {
  tm time_buf;
#ifdef CAF_MSVC
  localtime_s(&time_buf, &ts);
#else
  localtime_r(&ts, &time_buf);
#endif
  auto pos = strftime(buf, buf_size, "%FT%T", &time_buf);
  buf[pos++] = '.';
  if (ms > 0) {
    CAF_ASSERT(ms < 1000);
    buf[pos++] = static_cast<char>((ms / 100) + '0');
    buf[pos++] = static_cast<char>(((ms % 100) / 10) + '0');
    buf[pos++] = static_cast<char>((ms % 10) + '0');
  } else {
    for (int i = 0; i < 3; ++i)
      buf[pos++] = '0';
  }
  buf[pos] = '\0';
  return pos;
}

} // namespace caf::detail
