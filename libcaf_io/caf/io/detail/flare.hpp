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

#ifndef CAF_IO_DETAIL_FLARE_HPP
#define CAF_IO_DETAIL_FLARE_HPP

namespace caf {
namespace io {
namespace detail {

/// An object that can be used to signal a "ready" status via a file descriptor
/// that may be integrated with select(), poll(), etc. Though it may be used to
/// signal availability of a resource across threads, both access to that
/// resource and the use of the fire/extinguish functions must be performed in
/// a thread-safe manner in order for that to work correctly.
class flare {
public:
  /// Constructs a flare by opening a UNIX pipe.
  flare();

  flare(const flare&) = delete;
  flare& operator=(const flare&) = delete;

  /// Retrieves a file descriptor that will become ready if the flare has been
  /// "fired" and not yet "extinguishedd."
  int fd() const;

  /// Puts the object in the "ready" state by writing one byte into the
  /// underlying pipe.
  void fire();

  // Takes the object out of the "ready" state by consuming all bytes from the
  // underlying pipe.
  void extinguish();

  /// Attempts to consume only one byte from the pipe, potentially leaving the
  /// flare in "ready" state.
  /// @returns `true` if one byte was read successfully from the pipe and
  ///          `false` if the pipe had no data to be read.
  bool extinguish_one();

private:
  int fds_[2];
};

} // namespace detail
} // namespace io
} // namespace caf

#endif // CAF_IO_DETAIL_FLARE_HPP
