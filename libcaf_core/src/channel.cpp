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

#include "caf/channel.hpp"

#include "caf/actor.hpp"
#include "caf/group.hpp"
#include "caf/message.hpp"
#include "caf/actor_cast.hpp"
#include "caf/local_actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/abstract_group.hpp"
#include "caf/proxy_registry.hpp"

namespace caf {

channel::channel(const actor& x) : ptr_(actor_cast<abstract_channel*>(x)) {
  // nop
}

channel::channel(const group& x) :  ptr_(actor_cast<abstract_channel*>(x)) {
  // nop
}

channel::channel(const invalid_channel_t&) {
  // nop
}

intptr_t channel::compare(const abstract_channel* lhs,
                          const abstract_channel* rhs) noexcept {
  return reinterpret_cast<intptr_t>(lhs) - reinterpret_cast<intptr_t>(rhs);
}

channel::channel(abstract_channel* ptr) : ptr_(ptr) {
  // nop
}

channel::channel(local_actor* ptr) : ptr_(ptr) {
  // nop
}

channel& channel::operator=(const actor& x) {
  ptr_.reset(actor_cast<abstract_channel*>(x));
  return *this;
}

channel& channel::operator=(const group& x) {
  ptr_.reset(actor_cast<abstract_channel*>(x));
  return *this;
}

channel& channel::operator=(const invalid_channel_t&) {
  ptr_.reset();
  return *this;
}

intptr_t channel::compare(const channel& other) const noexcept {
  return compare(ptr_.get(), other.ptr_.get());
}

intptr_t channel::compare(const actor& other) const noexcept {
  return compare(ptr_.get(), actor_cast<abstract_actor_ptr>(other).get());
}

intptr_t channel::compare(const abstract_channel* other) const noexcept {
  return compare(ptr_.get(), other);
}

void serialize(serializer& sink, channel& x, const unsigned int) {
  if (sink.context() == nullptr)
    throw std::logic_error("Cannot serialize channel without context.");
  uint8_t flag = ! x ? 0 : (x->is_abstract_actor() ? 1 : 2);
  sink << flag;
  auto& sys = sink.context()->system();
  switch (flag) {
    case 1: {
      auto ptr = static_cast<abstract_actor*>(x.get());
      // register locally running actors to be able to deserialize them later
      if (sys.node() == ptr->node())
        sys.registry().put(ptr->id(), ptr->address());
      // we need lvalue references
      auto xid = ptr->id();
      auto xn = ptr->node();
      sink << xid << xn;
      break;
    }
    case 2: {
      auto ptr = static_cast<abstract_group*>(x.get());
      sink << ptr->module_name();
      ptr->save(sink);
      break;
    }
    default:
      ; // nop
  }
}

void serialize(deserializer& source, channel& x, const unsigned int) {
  if (source.context() == nullptr)
    throw std::logic_error("Cannot deserialize actor_addr without context.");
  uint8_t flag;
  source >> flag;
  auto& sys = source.context()->system();
  switch (flag) {
    case 1: {
      actor_id aid;
      node_id nid;
      source >> aid >> nid;
      // deal with local actors
      if (sys.node() == nid) {
        x = actor_cast<channel>(sys.registry().get(aid).first);
        return;
      }
      auto prp = source.context()->proxy_registry_ptr();
      if (prp == nullptr)
        throw std::logic_error("Cannot deserialize actor_addr of "
                               "remote node without proxy registry.");
      // deal with (proxies for) remote actors
      auto ptr = prp->get_or_put(nid, aid);
      if (ptr)
        x.ptr_ = std::move(ptr);
      else
        x = invalid_channel;
      break;
    }
    case 2: {
      std::string module_name;
      source >> module_name;
      auto mod = sys.groups().get_module(module_name);
      if (! mod)
        throw std::logic_error("Cannot deserialize a group for "
                               "unknown module: " + module_name);
      x = mod->load(source);
      break;
    }
    default:
      x = invalid_channel;
  }
}

std::string to_string(const channel& x) {
  if (! x)
    return "<invalid-channel>";
  auto ptr = actor_cast<abstract_channel*>(x);
  if (ptr->is_abstract_actor()) {
    intrusive_ptr<abstract_actor> aptr{static_cast<abstract_actor*>(ptr)};
    return to_string(actor_cast<actor>(aptr));
  }
  intrusive_ptr<abstract_group> gptr{static_cast<abstract_group*>(ptr)};
  return to_string(actor_cast<group>(gptr));
}

} // namespace caf
