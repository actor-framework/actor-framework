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

#include "caf/actor_control_block.hpp"

#include "caf/message.hpp"
#include "caf/actor_system.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/mailbox_element.hpp"

#include "caf/detail/disposer.hpp"

namespace caf {

actor_addr actor_control_block::address() {
  return {this, true};
}

void actor_control_block::enqueue(strong_actor_ptr sender, message_id mid,
                                  message content, execution_unit* host) {
  get()->enqueue(std::move(sender), mid, std::move(content), host);
}

void actor_control_block::enqueue(mailbox_element_ptr what,
                                  execution_unit* host) {
  get()->enqueue(std::move(what), host);
}

bool intrusive_ptr_upgrade_weak(actor_control_block* x) {
  auto count = x->strong_refs.load();
  while (count != 0)
    if (x->strong_refs.compare_exchange_weak(count, count + 1,
                                             std::memory_order_relaxed))
      return true;
  return false;
}

void intrusive_ptr_release_weak(actor_control_block* x) {
  // destroy object if last weak pointer expires
  if (x->weak_refs == 1
      || x->weak_refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
    x->block_dtor(x);
}

void intrusive_ptr_release(actor_control_block* x) {
  // release implicit weak pointer if the last strong ref expires
  // and destroy the data block
  if (x->strong_refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    x->data_dtor(x->get());
    intrusive_ptr_release_weak(x);
  }
}

bool operator==(const strong_actor_ptr& x, const abstract_actor* y) {
  return x.get() == actor_control_block::from(y);
}

bool operator==(const abstract_actor* x, const strong_actor_ptr& y) {
  return actor_control_block::from(x) == y.get();
}

template <class T>
void safe_actor(serializer& sink, T& storage) {
  CAF_LOG_TRACE(CAF_ARG(storage));
  if (! sink.context()) {
    CAF_LOG_ERROR("Cannot serialize actors without context.");
    CAF_RAISE_ERROR("Cannot serialize actors without context.");
  }
  auto& sys = sink.context()->system();
  auto ptr = storage.get();
  actor_id aid = invalid_actor_id;
  node_id nid = invalid_node_id;
  if (ptr) {
    aid = ptr->aid;
    nid = ptr->nid;
    // register locally running actors to be able to deserialize them later
    if (nid == sys.node())
      sys.registry().put(aid, actor_cast<strong_actor_ptr>(storage));
  }
  sink << aid << nid;
}

template <class T>
void load_actor(deserializer& source, T& storage) {
  CAF_LOG_TRACE("");
  storage.reset();
  if (! source.context())
    CAF_RAISE_ERROR("Cannot deserialize actor_addr without context.");
  auto& sys = source.context()->system();
  actor_id aid;
  node_id nid;
  source >> aid >> nid;
  CAF_LOG_DEBUG(CAF_ARG(aid) << CAF_ARG(nid));
  // deal with local actors
  if (sys.node() == nid) {
    storage = actor_cast<T>(sys.registry().get(aid));
    CAF_LOG_DEBUG("fetch actor handle from local actor registry: "
                  << (storage ? "found" : "not found"));
    return;
  }
  auto prp = source.context()->proxy_registry_ptr();
  if (! prp)
    CAF_RAISE_ERROR("Cannot deserialize remote actors without proxy registry.");
  // deal with (proxies for) remote actors
  storage = actor_cast<T>(prp->get_or_put(nid, aid));
}

void serialize(serializer& sink, strong_actor_ptr& x, const unsigned int) {
  CAF_LOG_TRACE("");
  safe_actor(sink, x);
}

void serialize(deserializer& source, strong_actor_ptr& x, const unsigned int) {
  CAF_LOG_TRACE("");
  load_actor(source, x);
}

void serialize(serializer& sink, weak_actor_ptr& x, const unsigned int) {
  CAF_LOG_TRACE("");
  safe_actor(sink, x);
}

void serialize(deserializer& source, weak_actor_ptr& x, const unsigned int) {
  CAF_LOG_TRACE("");
  load_actor(source, x);
}

std::string to_string(const actor_control_block* x) {
  if (! x)
    return "<invalid-actor>";
  std::string result = std::to_string(x->id());
  result += "@";
  result += to_string(x->node());
  return result;
}
std::string to_string(const strong_actor_ptr& x) {
  return to_string(x.get());
}

std::string to_string(const weak_actor_ptr& x) {
  return to_string(x.get());
}

} // namespace caf
