/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_ACTOR_OSTREAM_HPP
#define CAF_ACTOR_OSTREAM_HPP

#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/to_string.hpp"

namespace caf {

class local_actor;
class scoped_actor;

/// Provides support for thread-safe output operations on character streams. The
/// stream operates on a per-actor basis and will print only complete lines or
/// when explicitly forced to flush its buffer. The stream will convert *any*
/// operation to a message received by a printer actor. This actor is a sequence
/// point that ensures output appears never interleaved.
class actor_ostream {
public:
  using fun_type = actor_ostream& (*)(actor_ostream&);

  actor_ostream(actor_ostream&&) = default;
  actor_ostream(const actor_ostream&) = default;
  actor_ostream& operator=(actor_ostream&&) = default;
  actor_ostream& operator=(const actor_ostream&) = default;

  /// Open redirection file in append mode.
  static constexpr int append = 0x01;

  /// Creates a stream for `self`.
  explicit actor_ostream(actor self);

  /// Writes `arg` to the buffer allocated for the calling actor.
  actor_ostream& write(std::string arg);

  /// Flushes the buffer allocated for the calling actor.
  actor_ostream& flush();

  /// Redirects all further output from `src` to `file_name`.
  static void redirect(const actor& src, std::string file_name, int flags = 0);

  /// Redirects all further output from any actor that did not
  /// redirect its output to `file_name`.
  static void redirect_all(std::string file_name, int flags = 0);

  /// Writes `arg` to the buffer allocated for the calling actor.
  inline actor_ostream& operator<<(std::string arg) {
    return write(std::move(arg));
  }

  /// Writes `arg` to the buffer allocated for the calling actor.
  inline actor_ostream& operator<<(const char* arg) {
    return *this << std::string{arg};
  }

  /// Writes `to_string(arg)` to the buffer allocated for the calling actor,
  /// calling either `std::to_string` or `caf::to_string` depending on
  /// the argument.
  template <class T>
  inline typename std::enable_if<
    ! std::is_convertible<T, std::string>::value, actor_ostream&
  >::type operator<<(T&& arg) {
    using std::to_string;
    return write(to_string(std::forward<T>(arg)));
  }

  /// Apply `f` to `*this`.
  inline actor_ostream& operator<<(actor_ostream::fun_type f) {
    return f(*this);
  }

private:
  actor self_;
  actor printer_;
};

/// Convenience factory function for creating an actor output stream.
inline actor_ostream aout(actor self) {
  return actor_ostream{self};
}

} // namespace caf

namespace std {
// provide convenience overlaods for aout; implemented in logging.cpp
caf::actor_ostream& endl(caf::actor_ostream& o);
caf::actor_ostream& flush(caf::actor_ostream& o);
} // namespace std

#endif // CAF_ACTOR_OSTREAM_HPP
