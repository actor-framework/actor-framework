// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/critical.hpp"

#include "caf/config.hpp"

#include <cstdio>
#include <cstdlib>

#if defined(CAF_WINDOWS) || defined(CAF_BSD) || !__has_include(<execinfo.h>)

namespace caf::detail {

[[noreturn]] void critical(const char* msg, const std::source_location& loc,
                           int stack_offset) {
  size_t line = loc.line();
  fprintf(stderr, "[FATAL] critical error (%s:%zu): %s\n", loc.file_name(),
          line, msg);
  abort();
}

} // namespace caf::detail

#else // defined(CAF_LINUX) || defined(CAF_MACOS)

#  include <execinfo.h>

namespace caf::detail {

[[noreturn]] void critical(const char* msg, const std::source_location& loc,
                           int stack_offset) {
  size_t line = loc.line();
  fprintf(stderr, "[FATAL] critical error (%s:%zu): %s\n", loc.file_name(),
          line, msg);
  void* array[20];
  auto caf_bt_size = backtrace(array, 20);
  auto offset = stack_offset + 1;
  if (offset >= 0 && offset < caf_bt_size) {
    backtrace_symbols_fd(array + offset, caf_bt_size - offset, 2);
  }
  abort();
}

} // namespace caf::detail

#endif
