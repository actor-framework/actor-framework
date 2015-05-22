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

#ifndef CAF_MUTEX_HPP
#define CAF_MUTEX_HPP

#ifdef __RIOTBUILD_FLAG

#include "riot/mutex.hpp"

namespace caf {

using mutex = riot::mutex;
template <class Mutex> using lock_guard = riot::lock_guard<Mutex>;
template <class Mutex> using unique_lock = riot::unique_lock<Mutex>;

} // namespace caf

#else

#include <mutex>

namespace caf {

using mutex = std::mutex;
template <class Mutex> using lock_guard = std::lock_guard<Mutex>;
template <class Mutex> using unique_lock = std::unique_lock<Mutex>;

} // namespace caf

#endif

#endif // CAF_MUTEX_HPP
