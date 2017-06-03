/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <exception>

#include "caf/io/detail/flare.hpp"

namespace caf {
namespace io {
namespace detail {

flare::flare() {
  if (::pipe(fds_) == -1)
    std::terminate();
  ::fcntl(fds_[0], F_SETFD, ::fcntl(fds_[0], F_GETFD) | FD_CLOEXEC);
  ::fcntl(fds_[1], F_SETFD, ::fcntl(fds_[1], F_GETFD) | FD_CLOEXEC);
  ::fcntl(fds_[0], F_SETFL, ::fcntl(fds_[0], F_GETFL) | O_NONBLOCK);
  ::fcntl(fds_[1], F_SETFL, ::fcntl(fds_[1], F_GETFL) | O_NONBLOCK);
}

int flare::fd() const {
  return fds_[0];
}

void flare::fire() {
  char tmp = 0;
  for (;;) {
    auto n = ::write(fds_[1], &tmp, 1);
    if (n > 0)
      break; // Success -- wrote a byte to pipe.
    if (n < 0 && errno == EAGAIN)
      break; // Success -- pipe is full and just need at least one byte in it.
    // Loop, because either the byte wasn't written or we got EINTR.
  }
}

void flare::extinguish() {
  char tmp[256];
  for (;;)
    if (::read(fds_[0], tmp, sizeof(tmp)) == -1 && errno == EAGAIN)
      break; // Pipe is now drained.
}

bool flare::extinguish_one() {
  char tmp = 0;
  for (;;) {
    auto n = ::read(fds_[0], &tmp, 1);
    if (n == 1)
      return true; // Read one byte.
    if (n < 0 && errno == EAGAIN)
      return false; // No data available to read.
  }
}

} // namespace detail
} // namespace io
} // namespace caf
