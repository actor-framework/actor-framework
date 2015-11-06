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
#include "caf/actor_system.hpp"
#include "caf/deserializer.hpp"
#include "caf/proxy_registry.hpp"

#include "caf/logger.hpp"
#include "caf/actor_registry.hpp"

namespace caf {

proxy_registry::backend::~backend() {
  // nop
}

proxy_registry::proxy_entry::proxy_entry() : backend_(nullptr) {
  // nop
}

proxy_registry::proxy_entry::proxy_entry(actor_proxy::anchor_ptr ptr,
                                         backend& ref)
    : ptr_(std::move(ptr)),
      backend_(&ref) {
  // nop
}

proxy_registry::proxy_entry::~proxy_entry() {
  reset(exit_reason::remote_link_unreachable);
}

void proxy_registry::proxy_entry::reset(uint32_t rsn) {
  if (! ptr_ || ! backend_)
    return;
  auto ptr = ptr_->get();
  ptr_.reset();
  if (ptr)
    ptr->kill_proxy(backend_->registry_context(), rsn);
}

void proxy_registry::proxy_entry::assign(actor_proxy::anchor_ptr ptr,
                                         backend& ctx) {
  using std::swap;
  swap(ptr_, ptr);
  backend_ = &ctx;
}


proxy_registry::proxy_registry(actor_system& sys, backend& be)
    : system_(sys),
      backend_(be) {
  // nop
}

size_t proxy_registry::count_proxies(const key_type& node) {
  auto i = proxies_.find(node);
  return (i != proxies_.end()) ? i->second.size() : 0;
}

std::vector<actor_proxy_ptr> proxy_registry::get_all() const {
  std::vector<actor_proxy_ptr> result;
  for (auto& outer : proxies_) {
    for (auto& inner : outer.second) {
      if (inner.second) {
        auto ptr = inner.second->get();
        if (ptr)
          result.push_back(std::move(ptr));
      }
    }
  }
  return result;
}

std::vector<actor_proxy_ptr>
proxy_registry::get_all(const key_type& node) const {
  std::vector<actor_proxy_ptr> result;
  auto i = proxies_.find(node);
  if (i == proxies_.end())
    return result;
  auto& submap = i->second;
  for (auto& kvp : submap) {
    if (kvp.second) {
      auto ptr = kvp.second->get();
      if (ptr)
        result.push_back(std::move(ptr));
    }
  }
  return result;
}

actor_proxy_ptr proxy_registry::get(const key_type& node, actor_id aid) {
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

actor_proxy_ptr proxy_registry::get_or_put(const key_type& node, actor_id aid) {
  CAF_LOG_TRACE(CAF_ARG(node) << CAF_ARG(aid));
  auto& submap = proxies_[node];
  auto& anchor = submap[aid];
  actor_proxy_ptr result;
  CAF_LOG_DEBUG_IF(! anchor, "found no anchor in submap");
  if (anchor) {
    result = anchor->get();
    CAF_LOG_DEBUG_IF(result, "found valid anchor in submap");
    CAF_LOG_DEBUG_IF(! result, "found expired anchor in submap");
  }
  // replace anchor if we've created one using the default ctor
  // or if we've found an expired one in the map
  if (! anchor || ! result) {
    result = backend_.make_proxy(node, aid);
    CAF_LOG_WARNING_IF(! result, "backend could not create a proxy:"
                                 << CAF_ARG(node) << CAF_ARG(aid));
    if (result)
      anchor.assign(result->get_anchor(), backend_);
  }
  CAF_LOG_DEBUG_IF(result, CAF_ARG(result->address()));
  CAF_LOG_DEBUG_IF(! result, "result = nullptr");
  return result;
}

bool proxy_registry::empty() const {
  return proxies_.empty();
}

void proxy_registry::erase(const key_type& inf) {
  CAF_LOG_TRACE(CAF_ARG(inf));
  proxies_.erase(inf);
}

void proxy_registry::erase(const key_type& inf, actor_id aid, uint32_t rsn) {
  CAF_LOG_TRACE(CAF_ARG(inf) << CAF_ARG(aid));
  auto i = proxies_.find(inf);
  if (i != proxies_.end()) {
    auto& submap = i->second;
    auto j = submap.find(aid);
    if (j == submap.end())
      return;
    j->second.reset(rsn);
    submap.erase(j);
    if (submap.empty())
      proxies_.erase(i);
  }
}

void proxy_registry::clear() {
  proxies_.clear();
}

} // namespace caf
