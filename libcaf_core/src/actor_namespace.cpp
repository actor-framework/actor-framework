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

#include <utility>
#include <algorithm>

#include "caf/node_id.hpp"
#include "caf/actor_addr.hpp"
#include "caf/serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/actor_namespace.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {

actor_namespace::backend::~backend() {
  // nop
}

actor_namespace::actor_namespace(backend& be) : backend_(be) {
  // nop
}

void actor_namespace::write(serializer* sink, const actor_addr& addr) {
  CAF_ASSERT(sink != nullptr);
  if (! addr) {
    node_id::host_id_type zero;
    std::fill(zero.begin(), zero.end(), 0);
    sink->write_value(static_cast<actor_id>(0));         // actor id
    sink->write_raw(node_id::host_id_size, zero.data()); // host id
    sink->write_value(static_cast<uint32_t>(0));         // process id
  } else {
    // register locally running actors to be able to deserialize them later
    if (! addr.is_remote()) {
      auto reg = detail::singletons::get_actor_registry();
      reg->put(addr.id(), actor_cast<abstract_actor_ptr>(addr));
    }
    auto pinf = addr.node();
    sink->write_value(addr.id());                                  // actor id
    sink->write_raw(node_id::host_id_size, pinf.host_id().data()); // host id
    sink->write_value(pinf.process_id());                          // process id
  }
}

actor_addr actor_namespace::read(deserializer* source) {
  CAF_ASSERT(source != nullptr);
  node_id::host_id_type hid;
  auto aid = source->read<uint32_t>();                 // actor id
  source->read_raw(node_id::host_id_size, hid.data()); // host id
  auto pid = source->read<uint32_t>();                 // process id
  node_id this_node = detail::singletons::get_node_id();
  if (aid == 0 && pid == 0) {
    // 0:0 identifies an invalid actor
    return invalid_actor_addr;
  }
  if (pid == this_node.process_id() && hid == this_node.host_id()) {
    // identifies this exact process on this host, ergo: local actor
    auto a = detail::singletons::get_actor_registry()->get(aid);
    // might be invalid
    return a ? a->address() : invalid_actor_addr;
  }
  // identifies a remote actor; create proxy if needed
  return get_or_put({pid, hid}, aid)->address();
}

size_t actor_namespace::count_proxies(const key_type& node) {
  auto i = proxies_.find(node);
  return (i != proxies_.end()) ? i->second.size() : 0;
}

std::vector<actor_proxy_ptr> actor_namespace::get_all() const {
  std::vector<actor_proxy_ptr> result;
  for (auto& outer : proxies_) {
    for (auto& inner : outer.second) {
      auto ptr = inner.second->get();
      if (ptr) {
        result.push_back(std::move(ptr));
      }
    }
  }
  return result;
}

std::vector<actor_proxy_ptr>
actor_namespace::get_all(const key_type& node) const {
  std::vector<actor_proxy_ptr> result;
  auto i = proxies_.find(node);
  if (i == proxies_.end())
    return result;
  auto& submap = i->second;
  for (auto& kvp : submap) {
    auto ptr = kvp.second->get();
    if (ptr)
      result.push_back(std::move(ptr));
  }
  return result;
}

actor_proxy_ptr actor_namespace::get(const key_type& node, actor_id aid) {
  auto& submap = proxies_[node];
  auto i = submap.find(aid);
  if (i != submap.end()) {
    auto res = i->second->get();
    if (! res) {
      submap.erase(i); // instance is expired
    }
    return res;
  }
  return nullptr;
}

actor_proxy_ptr actor_namespace::get_or_put(const key_type& node,
                                            actor_id aid) {
  auto& submap = proxies_[node];
  auto& anchor = submap[aid];
  actor_proxy_ptr result;
  if (anchor) {
    result = anchor->get();
  }
  // replace anchor if we've created one using the default ctor
  // or if we've found an expired one in the map
  if (! anchor || ! result) {
    result = backend_.make_proxy(node, aid);
    anchor = result->get_anchor();
  }
  return result;
}

bool actor_namespace::empty() const {
  return proxies_.empty();
}

void actor_namespace::erase(const key_type& inf) {
  CAF_LOG_TRACE(CAF_TARG(inf, to_string));
  proxies_.erase(inf);
}

void actor_namespace::erase(const key_type& inf, actor_id aid) {
  CAF_LOG_TRACE(CAF_TARG(inf, to_string) << ", " << CAF_ARG(aid));
  auto i = proxies_.find(inf);
  if (i != proxies_.end()) {
    i->second.erase(aid);
    if (i->second.empty()) {
      proxies_.erase(i);
    }
  }
}

void actor_namespace::clear() {
  proxies_.clear();
}

} // namespace caf
