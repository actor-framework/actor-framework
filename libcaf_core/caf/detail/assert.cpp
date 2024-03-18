// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/assert.hpp"

#include "caf/config.hpp"

#include <cstdio>
#include <cstdlib>

#if defined(CAF_WINDOWS) || defined(CAF_BSD) || !__has_include(<execinfo.h>)

namespace caf::detail {

[[noreturn]] void assertion_failed(const char* file, int line,
                                   const char* stmt) {
  fprintf(stderr, "%s:%u: assertion '%s' failed\n", file, line, stmt);
  ::abort();
}

} // namespace caf::detail

#else // defined(CAF_LINUX) || defined(CAF_MACOS)

#  include <execinfo.h>

namespace caf::detail {

[[noreturn]] void assertion_failed(const char* file, int line,
                                   const char* stmt) {
  fprintf(stderr, "%s:%u: assertion '%s' failed\n", file, line, stmt);
  void* array[20];
  auto caf_bt_size = ::backtrace(array, 20);
  ::backtrace_symbols_fd(array + 1, caf_bt_size - 1, 2);
  ::abort();
}

} // namespace caf::detail

#endif
