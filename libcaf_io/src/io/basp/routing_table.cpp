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

routing_table::routing_table(abstract_broker* parent) : parent_(parent) {
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
  return {};
}

optional<connection_handle>
routing_table::lookup_direct(const node_id& nid) const {
  std::unique_lock<std::mutex> guard{mtx_};
  auto i = direct_by_nid_.find(nid);
  if (i != direct_by_nid_.end())
    return i->second;
  return {};
}

node_id routing_table::lookup_indirect(const node_id& nid) const {
  std::unique_lock<std::mutex> guard{mtx_};
  auto i = indirect_.find(nid);
  if (i == indirect_.end())
    return {};
  if (!i->second.empty())
    return *i->second.begin();
  return {};
}

node_id routing_table::erase_direct(const connection_handle& hdl) {
  std::unique_lock<std::mutex> guard{mtx_};
  // Sanity check: do nothing if the handle is not mapped to a node.
  auto i = direct_by_hdl_.find(hdl);
  if (i == direct_by_hdl_.end())
    return {};
  // We always remove i from direct_by_hdl_.
  auto node = std::move(i->second);
  direct_by_hdl_.erase(i);
  // Sanity check: direct_by_nid_ should contain a reverse mapping.
  auto j = direct_by_nid_.find(node);
  if (j == direct_by_nid_.end()) {
    CAF_LOG_WARNING("no reverse mapping exists for the connection handle");
    return node;
  }
  // Try to find an alternative connection for communicating with the node.
  auto predicate = [&](const handle_to_node_map::value_type& kvp) {
    return kvp.second == node;
  };
  auto e = direct_by_hdl_.end();
  auto alternative = std::find_if(direct_by_hdl_.begin(), e, predicate);
  if (alternative != e) {
    // Update node <-> handle mapping.
    j->second = alternative->first;
    return {};
  } else {
    // Drop the node after losing the last connection to it.
    direct_by_nid_.erase(j);
    return node;
  }
}

bool routing_table::erase_indirect(const node_id& dest) {
  std::unique_lock<std::mutex> guard{mtx_};
  auto i = indirect_.find(dest);
  if (i == indirect_.end())
    return false;
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
}

void routing_table::add_alternative(const connection_handle& hdl,
                                    const node_id& nid) {
  std::unique_lock<std::mutex> guard{mtx_};
  CAF_ASSERT(direct_by_nid_.count(nid) != 0);
  // This member function is safe to call repeatedly. Hence, we ignore the
  // result of emplace on purpose.
  direct_by_hdl_.emplace(hdl, nid);
}

void routing_table::select_alternative(const connection_handle& hdl,
                                       const node_id& nid) {
  std::unique_lock<std::mutex> guard{mtx_};
  CAF_ASSERT(direct_by_hdl_[hdl] == nid);
  direct_by_nid_[nid] = hdl;
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
  hops.emplace(hop);
  return result;
}

} // namespace basp
} // namespace io
} // namespace caf
