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

node_id routing_table::lookup(const endpoint_handle& hdl) const {
  return get_opt(nid_by_hdl_, hdl, none);
}

routing_table::lookup_result routing_table::lookup(const node_id& nid) const {
  auto i = node_information_base_.find(nid);
  if (i != node_information_base_.end())
    return {true, i->second.hdl};
  return {false, none};
}

void routing_table::erase(const endpoint_handle& hdl, erase_callback& cb) {
  auto i = nid_by_hdl_.find(hdl);
  if (i == nid_by_hdl_.end())
    return;
  cb(i->second);
  parent_->parent().notify<hook::connection_lost>(i->second);
  // TODO: Should we keep address information and set the state to 'none'?
  node_information_base_.erase(i->second);
  //hdl_by_nid_.erase(i->second);
  nid_by_hdl_.erase(i->first);
}

void routing_table::add(const endpoint_handle& hdl, const node_id& nid) {
  CAF_ASSERT(nid_by_hdl_.count(hdl) == 0);
  //CAF_ASSERT(hdl_by_nid_.count(nid) == 0);
  CAF_ASSERT(node_information_base_.count(nid) == 0);
  nid_by_hdl_.emplace(hdl, nid);
  //hdl_by_nid_.emplace(nid, hdl);
  node_information_base_[nid] = node_info{hdl, {}, none};
  parent_->parent().notify<hook::new_connection_established>(nid);
}

void routing_table::add(const node_id& nid) {
  //CAF_ASSERT(hdl_by_nid_.count(nid) == 0);
  CAF_ASSERT(node_information_base_.count(nid) == 0);
  node_information_base_[nid] = node_info{none, {}, none};
  // TODO: Some new related hook?
  //parent_->parent().notify<hook::new_connection_established>(nid);
}

bool routing_table::reachable(const node_id& dest) {
  auto i = node_information_base_.find(dest);
  if (i != node_information_base_.end())
    return i->second.hdl != none;
  return false;
}

bool routing_table::forwarder(const node_id& nid,
                              routing_table::endpoint_handle origin) {
  auto i = node_information_base_.find(nid);
  if (i == node_information_base_.end())
    return false;
  i->second.origin = origin;
  return true;
}

optional<routing_table::endpoint_handle>
routing_table::forwarder(const node_id& nid) {
  auto i = node_information_base_.find(nid);
  if (i == node_information_base_.end())
    return none;
  return i->second.origin;
}

bool routing_table::addresses(const node_id& nid,
                              network::address_listing addrs) {
  auto i = node_information_base_.find(nid);
  if (i == node_information_base_.end())
    return false;
  for (auto& p : addrs) {
    auto& existing = i->second.addrs[p.first];
    existing.insert(existing.end(), p.second.begin(), p.second.end());
  }
  return true;
}

optional<const network::address_listing&>
routing_table::addresses(const node_id& nid) {
  auto i = node_information_base_.find(nid);
  if (i == node_information_base_.end())
    return none;
  return i->second.addrs;
}

} // namespace basp
} // namespace io
} // namespace caf
