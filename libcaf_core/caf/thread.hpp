/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 * Raphael Hiesgen <raphael.hiesgen (at) haw-hamburg.de>                      *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_THREAD_HPP
#define CAF_THREAD_HPP

#ifdef __RIOTBUILD_FLAG

#include "riot/thread.hpp"

namespace caf {

using thread = riot::thread;

namespace this_thread {

using riot::this_thread::get_id;
using riot::this_thread::yield;
using riot::this_thread::sleep_for;

} // namespace this_thread

} // namespace caf

#else

#include <time.h>

#include <thread>

namespace caf {

using thread = std::thread;

namespace this_thread {

using std::this_thread::get_id;

// GCC hack
#if !defined(_GLIBCXX_USE_SCHED_YIELD) && !defined(__clang__)
inline void yield() noexcept {
  timespec req;
  req.tv_sec = 0;
  req.tv_nsec = 1;
  nanosleep(&req, nullptr);
}
#else
using std::this_thread::yield;
#endif

// another GCC hack
#if !defined(_GLIBCXX_USE_NANOSLEEP) && !defined(__clang__)
template <class Rep, typename Period>
inline void sleep_for(const chrono::duration<Rep, Period>& rt) {
  auto sec = chrono::duration_cast<chrono::seconds>(rt);
  auto nsec = chrono::duration_cast<chrono::nanoseconds>(rt - sec);
  timespec req;
  req.tv_sec = sec.count();
  req.tv_nsec = nsec.count();
  nanosleep(&req, nullptr);
}
#else
using std::this_thread::sleep_for;
#endif

} // namespace this_thread

} // namespace caf

#endif

#endif // CAF_THREAD_HPP
