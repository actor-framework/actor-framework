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

#pragma once

#include <unordered_map>
#include <unordered_set>

#include "caf/callback.hpp"
#include "caf/io/abstract_broker.hpp"
#include "caf/io/basp/buffer_type.hpp"
#include "caf/io/network/interfaces.hpp"
#include "caf/node_id.hpp"

namespace caf {
namespace io {
namespace basp {

/// @addtogroup BASP

/// Stores routing information for a single broker participating as
/// BASP peer and provides both direct and indirect paths.
class routing_table {
public:
  using endpoint = std::pair<uint16_t, network::address_listing>;

  explicit routing_table(abstract_broker* parent);

  virtual ~routing_table();

  /// Result for a lookup of a node.
  struct lookup_result {
    /// Tracks whether the node is already known.
    bool known;
    /// Servant handle to communicate with the node -- if already created.
    optional<connection_handle> hdl;
  };

  /// Describes a function object for erase operations that
  /// is called for each indirectly lost connection.
  using erase_callback = callback<const node_id&>;

  /// Returns the ID of the peer reachable via `hdl` or
  /// `none` if `hdl` is unknown.
  node_id lookup(const connection_handle& hdl) const;

  /// Returns the state for communication with `nid` along with a handle
  /// if communication is established or `none` if `nid` is unknown.
  lookup_result lookup(const node_id& nid) const;

  /// Adds a new endpoint to the table.
  /// @pre `hdl != invalid_connection_handle && nid != none`
  void add(const node_id& nid, const connection_handle& hdl);

  /// Add a new endpoint to the table.
  /// @pre `origin != none && nid != none`
  void add(const node_id& nid, const node_id& origin);

  /// Adds a new endpoint to the table that has no attached information.
  /// that propagated information about the node.
  /// @pre `nid != none`
  void add(const node_id& nid);

  /// Removes a direct connection and calls `cb` for any node
  /// that became unreachable as a result of this operation,
  /// including the node that is assigned as direct path for `hdl`.
  void erase(const connection_handle& hdl, erase_callback& cb);

  /// Queries whether `dest` is reachable directly.
  bool reachable(const node_id& dest);

  /// Returns the parent broker.
  inline abstract_broker* parent() {
    return parent_;
  }

  /// Set the forwarding node that first mentioned `hdl`.
  bool origin(const node_id& nid, const node_id& origin);

  /// Get the forwarding node that first mentioned `hdl`
  /// or `none` if the node is unknown.
  optional<node_id> origin(const node_id& nid);

  /// Set the handle for communication with `nid`.
  bool handle(const node_id& nid, const connection_handle& hdl);

  /// Get the handle for communication with `nid`
  /// or `none` if the node is unknown.
  optional<connection_handle> handle(const node_id& nid);

  /// Get the addresses to reach `nid` or `none` if the node is unknown.
  optional<const endpoint&> address(const node_id& nid);

  /// Returns the local autoconnect endpoint.
  const endpoint& autoconnect_endpoint();

  /// Set the local autoconenct endpoint.
  void autoconnect_endpoint(uint16_t, network::address_listing);

public:
  /// Entry to bundle information for a remote endpoint.
  struct node_info {
    /// Handle for the node if communication is established.
    optional<connection_handle> hdl;
    /// The endpoint who told us about the node.
    optional<node_id> origin;
  };

  template <class Map, class Fallback>
  typename Map::mapped_type
  get_opt(const Map& m, const typename Map::key_type& k, Fallback&& x) const {
    auto i = m.find(k);
    if (i != m.end())
      return i->second;
    return std::forward<Fallback>(x);
  }

  abstract_broker* parent_;
  std::unordered_map<connection_handle, node_id> nid_by_hdl_;
  std::unordered_map<node_id, node_info> node_information_base_;
  endpoint autoconnect_endpoint_;
};

/// @}

} // namespace basp
} // namespace io
} // namespace caf

