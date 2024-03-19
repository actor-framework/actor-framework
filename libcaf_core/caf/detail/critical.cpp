// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/critical.hpp"

#include <cstdio>
#include <cstdlib>

namespace caf::detail {

[[noreturn]] void critical(const char* file, int line, const char* msg) {
  fprintf(stderr, "[FATAL] critical error (%s:%d): %s\n", file, line, msg);
  abort();
}

} // namespace caf::detail
