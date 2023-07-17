// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/set_thread_name.hpp"

#include "caf/config.hpp"

#ifndef CAF_WINDOWS
#  include <pthread.h>
#endif // CAF_WINDOWS

#if defined(CAF_LINUX)
#  include <sys/prctl.h>
#elif defined(CAF_BSD) && !defined(CAF_NET_BSD)
#  include <pthread_np.h>
#endif // defined(...)

#include <thread>
#include <type_traits>

namespace caf::detail {

void set_thread_name(const char* name) {
  CAF_IGNORE_UNUSED(name);
#ifdef CAF_WINDOWS
  // nop
#else // CAF_WINDOWS
  static_assert(std::is_same_v<std::thread::native_handle_type, pthread_t>,
                "std::thread not based on pthread_t");
#  if defined(CAF_MACOS)
  pthread_setname_np(name);
#  elif defined(CAF_LINUX)
  prctl(PR_SET_NAME, name, 0, 0, 0);
#  elif defined(CAF_NET_BSD)
  pthread_setname_np(pthread_self(), name, NULL);
#  elif defined(CAF_BSD)
  pthread_set_name_np(pthread_self(), name);
#  endif // defined(...)
#endif   // CAF_WINDOWS
}

} // namespace caf::detail
