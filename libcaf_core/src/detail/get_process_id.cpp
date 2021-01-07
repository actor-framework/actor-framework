// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/get_process_id.hpp"

#include "caf/config.hpp"

#ifdef CAF_WINDOWS
#  include <windows.h>
#else
#  include <sys/types.h>
#  include <unistd.h>
#endif

namespace caf::detail {

unsigned get_process_id() {
#ifdef CAF_WINDOWS
  return GetCurrentProcessId();
#else
  return static_cast<unsigned>(getpid());
#endif
}

} // namespace caf::detail
