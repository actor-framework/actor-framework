/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

class actor_ostream {

 public:

  using fun_type = actor_ostream& (*)(actor_ostream&);

  actor_ostream(actor_ostream&&) = default;
  actor_ostream(const actor_ostream&) = default;

  actor_ostream& operator=(actor_ostream&&) = default;
  actor_ostream& operator=(const actor_ostream&) = default;

  explicit actor_ostream(actor self);

  actor_ostream& write(std::string arg);

  actor_ostream& flush();

  inline actor_ostream& operator<<(std::string arg) {
    return write(std::move(arg));
  }

  inline actor_ostream& operator<<(const message& arg) {
    return write(caf::to_string(arg));
  }

  // disambiguate between conversion to string and to message
  inline actor_ostream& operator<<(const char* arg) {
    return *this << std::string{arg};
  }

  template <class T>
  inline typename std::enable_if<
    !std::is_convertible<T, std::string>::value &&
    !std::is_convertible<T, message>::value, actor_ostream&
  >::type operator<<(T&& arg) {
    return write(std::to_string(std::forward<T>(arg)));
  }

  inline actor_ostream& operator<<(actor_ostream::fun_type f) {
    return f(*this);
  }

 private:

  actor m_self;
  actor m_printer;

};

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
