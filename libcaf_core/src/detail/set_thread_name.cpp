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

#include "caf/detail/set_thread_name.hpp"

#include "caf/config.hpp"

#ifndef CAF_WINDOWS
#include <pthread.h>
#endif // CAF_WINDOWS

#if defined(CAF_LINUX)
#include <sys/prctl.h>
#elif defined(CAF_BSD)
#include <pthread_np.h>
#endif // defined(...)

#include <thread>
#include <type_traits>

namespace caf {
namespace detail {

void set_thread_name(const char* name) {
  CAF_IGNORE_UNUSED(name);
#ifdef CAF_WINDOWS
  // nop
#else // CAF_WINDOWS
  static_assert(std::is_same<std::thread::native_handle_type, pthread_t>::value,
                "std::thread not based on pthread_t");
#if defined(CAF_MACOS)
  pthread_setname_np(name);
#elif defined(CAF_LINUX)
  prctl(PR_SET_NAME, name, 0, 0, 0);
#elif defined(CAF_BSD)
  pthread_set_name_np(pthread_self(), name);
#endif // defined(...)
#endif // CAF_WINDOWS
}

} // namespace detail
} // namespace caf
