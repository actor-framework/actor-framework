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

#include "caf/actor_ostream.hpp"

#include "caf/send.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/abstract_actor.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {

actor_ostream::actor_ostream(abstract_actor* self)
    : self_(self),
      printer_(self->home_system().scheduler().printer()) {
  // nop
}

actor_ostream& actor_ostream::write(std::string arg) {
  send_as(actor_cast<actor>(self_), printer_, add_atom::value, std::move(arg));
  return *this;
}

actor_ostream& actor_ostream::flush() {
  send_as(actor_cast<actor>(self_), printer_, flush_atom::value);
  return *this;
}

void actor_ostream::redirect(abstract_actor* self, std::string fn, int flags) {
  if (! self)
    return;
  intrusive_ptr<abstract_actor> ptr{self};
  send_as(actor_cast<actor>(ptr), self->home_system().scheduler().printer(),
          redirect_atom::value, self->address(), std::move(fn), flags);
}

void actor_ostream::redirect_all(actor_system& sys, std::string fn, int flags) {
  anon_send(sys.scheduler().printer(),
            redirect_atom::value, std::move(fn), flags);
}

actor_ostream aout(abstract_actor* self) {
  return actor_ostream{self};
}

actor_ostream aout(scoped_actor& self) {
  return actor_ostream{self.get()};
}

} // namespace caf

namespace std {

caf::actor_ostream& endl(caf::actor_ostream& o) {
  return o.write("\n");
}

caf::actor_ostream& flush(caf::actor_ostream& o) {
  return o.flush();
}

} // namespace std
