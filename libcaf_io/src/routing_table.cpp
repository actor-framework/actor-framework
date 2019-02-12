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

#include "caf/io/basp/routing_table.hpp"

#include "caf/io/middleman.hpp"

namespace caf {
namespace io {
namespace basp {

routing_table::routing_table(abstract_broker* parent)
  : parent_(parent) {
  // nop
}

routing_table::~routing_table() {
  // nop
}

optional<routing_table::route> routing_table::lookup(const node_id& target) {
  std::unique_lock<std::mutex> guard{mtx_};
  // Check whether we have a direct path first.
  { // Lifetime scope of first iterator.
    auto i = direct_by_nid_.find(target);
    if (i != direct_by_nid_.end())
      return route{target, i->second};
  }
  // Pick first available indirect route.
  auto i = indirect_.find(target);
  if (i != indirect_.end()) {
    auto& hops = i->second;
    while (!hops.empty()) {
      auto& hop = *hops.begin();
      auto j = direct_by_nid_.find(hop);
      if (j != direct_by_nid_.end())
        return route{hop, j->second};
      // Erase hops that became invalid.
      hops.erase(hops.begin());
    }
  }
  return none;
}

node_id routing_table::lookup_direct(const connection_handle& hdl) const {
  std::unique_lock<std::mutex> guard{mtx_};
  auto i = direct_by_hdl_.find(hdl);
  if (i != direct_by_hdl_.end())
    return i->second;
  return none;
}

optional<connection_handle>
routing_table::lookup_direct(const node_id& nid) const {
  std::unique_lock<std::mutex> guard{mtx_};
  auto i = direct_by_nid_.find(nid);
  if (i != direct_by_nid_.end())
    return i->second;
  return none;
}

node_id routing_table::lookup_indirect(const node_id& nid) const {
  std::unique_lock<std::mutex> guard{mtx_};
  auto i = indirect_.find(nid);
  if (i == indirect_.end())
    return none;
  if (!i->second.empty())
    return *i->second.begin();
  return none;
}

node_id routing_table::erase_direct(const connection_handle& hdl) {
  std::unique_lock<std::mutex> guard{mtx_};
  auto i = direct_by_hdl_.find(hdl);
  if (i == direct_by_hdl_.end())
    return none;
  direct_by_nid_.erase(i->second);
  node_id result = std::move(i->second);
  direct_by_hdl_.erase(i->first);
  return result;
}

bool routing_table::erase_indirect(const node_id& dest) {
  std::unique_lock<std::mutex> guard{mtx_};
  auto i = indirect_.find(dest);
  if (i == indirect_.end())
    return false;
  if (parent_->parent().has_hook())
    for (auto& nid : i->second)
      parent_->parent().notify<hook::route_lost>(nid, dest);
  indirect_.erase(i);
  return true;
}

void routing_table::add_direct(const connection_handle& hdl,
                               const node_id& nid) {
  std::unique_lock<std::mutex> guard{mtx_};
  auto hdl_added = direct_by_hdl_.emplace(hdl, nid).second;
  auto nid_added = direct_by_nid_.emplace(nid, hdl).second;
  CAF_ASSERT(hdl_added && nid_added);
  CAF_IGNORE_UNUSED(hdl_added);
  CAF_IGNORE_UNUSED(nid_added);
  parent_->parent().notify<hook::new_connection_established>(nid);
}

bool routing_table::add_indirect(const node_id& hop, const node_id& dest) {
  std::unique_lock<std::mutex> guard{mtx_};
  // Never add indirect entries if we already have direct connection.
  if (direct_by_nid_.count(dest) != 0)
    return false;
  // Never add indirect entries if we don't have a connection to the hop.
  if (direct_by_nid_.count(hop) == 0)
    return false;
  // Add entry to our node ID set.
  auto& hops = indirect_[dest];
  auto result = hops.empty();
  if (hops.emplace(hop).second)
    parent_->parent().notify<hook::new_route_added>(hop, dest);
  return result;
}

} // namespace basp
} // namespace io
} // namespace caf
