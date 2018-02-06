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

#include "caf/actor_ostream.hpp"

#include "caf/send.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/default_attachable.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {

actor_ostream::actor_ostream(local_actor* self)
    : self_(self->id()),
      printer_(self->home_system().scheduler().printer()) {
  init(self);
}

actor_ostream::actor_ostream(scoped_actor& self)
    : self_(self->id()),
      printer_(self->home_system().scheduler().printer()) {
  init(actor_cast<abstract_actor*>(self));
}

actor_ostream& actor_ostream::write(std::string arg) {
  printer_->enqueue(make_mailbox_element(nullptr, make_message_id(), {},
                                         add_atom::value, self_,
                                         std::move(arg)),
                    nullptr);
  return *this;
}

actor_ostream& actor_ostream::flush() {
  printer_->enqueue(make_mailbox_element(nullptr, make_message_id(), {},
                                          flush_atom::value, self_),
                    nullptr);
  return *this;
}

void actor_ostream::redirect(abstract_actor* self, std::string fn, int flags) {
  if (self == nullptr)
    return;
  auto pr = self->home_system().scheduler().printer();
  pr->enqueue(make_mailbox_element(nullptr, make_message_id(), {},
                                    redirect_atom::value, self->id(),
                                    std::move(fn), flags),
              nullptr);
}

void actor_ostream::redirect_all(actor_system& sys, std::string fn, int flags) {
  auto pr = sys.scheduler().printer();
  pr->enqueue(make_mailbox_element(nullptr, make_message_id(), {},
                                    redirect_atom::value,
                                    std::move(fn), flags),
              nullptr);
}

void actor_ostream::init(abstract_actor* self) {
  if (!self->getf(abstract_actor::has_used_aout_flag))
    self->setf(abstract_actor::has_used_aout_flag);
}

actor_ostream aout(local_actor* self) {
  return actor_ostream{self};
}

actor_ostream aout(scoped_actor& self) {
  return actor_ostream{self};
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
