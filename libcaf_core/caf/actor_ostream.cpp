// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_ostream.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/scheduler.hpp"
#include "caf/scoped_actor.hpp"

namespace caf {

actor_ostream::actor_ostream(local_actor* self)
  : printer_(self->home_system().scheduler().printer_for(self)) {
  // nop
}

actor_ostream::actor_ostream(scoped_actor& self)
  : actor_ostream(
    static_cast<local_actor*>(actor_cast<abstract_actor*>(self))) {
  // nop
}

void actor_ostream::redirect(abstract_actor* self, std::string fn, int flags) {
  if (self == nullptr)
    return;
  auto pr = self->home_system().printer();
  if (!pr)
    return;
  pr->enqueue(make_mailbox_element(nullptr, make_message_id(), redirect_atom_v,
                                   self->id(), std::move(fn), flags),
              nullptr);
}

void actor_ostream::redirect_all(actor_system& sys, std::string fn, int flags) {
  auto pr = sys.printer();
  if (!pr)
    return;
  pr->enqueue(make_mailbox_element(nullptr, make_message_id(), redirect_atom_v,
                                   std::move(fn), flags),
              nullptr);
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
