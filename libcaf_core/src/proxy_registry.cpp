/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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
#include "caf/actor_system.hpp"
#include "caf/deserializer.hpp"
#include "caf/proxy_registry.hpp"

#include "caf/logger.hpp"
#include "caf/actor_registry.hpp"

namespace caf {

proxy_registry::backend::~backend() {
  // nop
}

proxy_registry::proxy_registry(actor_system& sys, backend& be)
    : system_(sys),
      backend_(be) {
  // nop
}

proxy_registry::~proxy_registry() {
  clear();
}

size_t proxy_registry::count_proxies(const node_id& node) {
  auto i = proxies_.find(node);
  return (i != proxies_.end()) ? i->second.size() : 0;
}

strong_actor_ptr proxy_registry::get(const node_id& node, actor_id aid) {
  auto& submap = proxies_[node];
  auto i = submap.find(aid);
  if (i != submap.end())
    return i->second;
  return nullptr;
}

strong_actor_ptr proxy_registry::get_or_put(const node_id& nid, actor_id aid) {
  CAF_LOG_TRACE(CAF_ARG(nid) << CAF_ARG(aid));
  auto& result = proxies_[nid][aid];
  if (!result)
    result = backend_.make_proxy(nid, aid);
  return result;
}

std::vector<strong_actor_ptr> proxy_registry::get_all(const node_id& node) {
  std::vector<strong_actor_ptr> result;
  auto i = proxies_.find(node);
  if (i != proxies_.end())
    for (auto& kvp : i->second)
      result.push_back(kvp.second);
  return result;
}

bool proxy_registry::empty() const {
  return proxies_.empty();
}

void proxy_registry::erase(const node_id& nid) {
  CAF_LOG_TRACE(CAF_ARG(nid));
  auto i = proxies_.find(nid);
  if (i == proxies_.end())
    return;
  for (auto& kvp : i->second)
    kill_proxy(kvp.second, exit_reason::remote_link_unreachable);
  proxies_.erase(i);
}

void proxy_registry::erase(const node_id& nid, actor_id aid, error rsn) {
  CAF_LOG_TRACE(CAF_ARG(nid) << CAF_ARG(aid));
  auto i = proxies_.find(nid);
  if (i != proxies_.end()) {
    auto& submap = i->second;
    auto j = submap.find(aid);
    if (j == submap.end())
      return;
    kill_proxy(j->second, std::move(rsn));
    submap.erase(j);
    if (submap.empty())
      proxies_.erase(i);
  }
}

void proxy_registry::clear() {
  for (auto& kvp : proxies_)
    for (auto& sub_kvp : kvp.second)
      kill_proxy(sub_kvp.second, exit_reason::remote_link_unreachable);
  proxies_.clear();
}

void proxy_registry::kill_proxy(strong_actor_ptr& ptr, error rsn) {
  if (!ptr)
    return;
  auto pptr = static_cast<actor_proxy*>(actor_cast<abstract_actor*>(ptr));
  pptr->kill_proxy(backend_.registry_context(), std::move(rsn));
}

} // namespace caf
