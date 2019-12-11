#include "caf/detail/affinity.hpp"

#include <iostream>
#include <set>

#include "caf/config.hpp"

#ifdef CAF_LINUX
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif // _GNU_SOURCE
#  include <sched.h>
#endif // CAF_LINUX

#ifdef CAF_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define _GNU_SOURCE
#  endif // WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif // CAF_WINDOWS

#ifdef CAF_MACOS
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif // _GNU_SOURCE
#  include <mach/mach_init.h>
#  include <mach/thread_act.h>
#  include <pthread.h>
#endif // CAF_MAC

#ifdef CAF_BSD
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif // _GNU_SOURCE
#  include <pthread.h>
#endif // CAF_BSD

namespace caf::detail {

void set_current_thread_affinity(const core_group& cores) {
  if (cores.size() == 0) {
    return;
  }
#if defined(CAF_LINUX)
  // Create a cpu_set_t object representing a set of CPUs.
  // Clear it and mark only CPU i as set.
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  for (int const& c : cores) {
    CPU_SET(c, &cpuset);
  }
  if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) < 0) {
    std::cerr << "Error setting affinity" << std::endl;
  }
#elif defined(CAF_WINDOWS)
  HANDLE thread_ptr = GetCurrentThread();
  // Create the affinity mask
  DWORD_PTR mask = 0;
  for (int const& c : cores) {
    mask |= (static_cast<DWORD_PTR>(1) << c);
  };
  if (SetThreadAffinityMask(thread_ptr, mask) == 0) {
    std::cerr << "Error setting affinity" << std::endl;
  }
  CloseHandle(thread_ptr);
#elif defined(CAF_MACOS)
  auto thread_id = pthread_mach_thread_np(pthread_self());
  // Create the affinity policy with only the first element of the set.
  // MacOS does not support to pin a thread on multiple cores
  thread_affinity_policy affinity;
  affinity.affinity_tag = *(cores.begin());
  if (thread_policy_set(thread_id, THREAD_AFFINITY_POLICY,
                        (thread_policy_t) &affinity,
                        THREAD_AFFINITY_POLICY_COUNT)
      != 0) {
    std::cerr << "Error setting affinity" << std::endl;
  }
#elif defined(CAF_BSD)
  auto thread_id = pthread_self();
  // Create a cpu_set_t object representing a set of CPUs.
  // Clear it and mark only CPU i as set.
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  for (int const& c : cores) {
    CPU_SET(c, &cpuset);
  }
  if (pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset) != 0) {
    std::cerr << "Error setting affinity" << std::endl;
  }
#else
  std::cerr << "Thread affinity not supported in this platform" << std::endl;
#endif
}

} // namespace caf::detail
