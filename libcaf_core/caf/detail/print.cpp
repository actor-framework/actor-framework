// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/print.hpp"

#include "caf/config.hpp"
#include "caf/detail/assert.hpp"

namespace caf::detail {

size_t print_timestamp(char* buf, size_t buf_size, time_t ts, size_t ms) {
  tm time_buf;
#ifdef CAF_WINDOWS
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
