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
#include "caf/node_id.hpp"

namespace caf {
namespace io {
namespace basp {

/// @addtogroup BASP

/// Stores routing information for a single broker participating as
/// BASP peer and provides both direct and indirect paths.
class routing_table {
public:

  explicit routing_table(abstract_broker* parent);

  virtual ~routing_table();

  /// Describes a routing path to a node.
  struct route {
    const node_id& next_hop;
    connection_handle hdl;
  };

  /// Describes a function object for erase operations that
  /// is called for each indirectly lost connection.
  using erase_callback = callback<const node_id&>;

  /// Returns a route to `target` or `none` on error.
  optional<route> lookup(const node_id& target);

  /// Returns the ID of the peer connected via `hdl` or
  /// `none` if `hdl` is unknown.
  node_id lookup_direct(const connection_handle& hdl) const;

  /// Returns the handle offering a direct connection to `nid` or
  /// `invalid_connection_handle` if no direct connection to `nid` exists.
  optional<connection_handle> lookup_direct(const node_id& nid) const;

  /// Returns the next hop that would be chosen for `nid`
  /// or `none` if there's no indirect route to `nid`.
  node_id lookup_indirect(const node_id& nid) const;

  /// Adds a new direct route to the table.
  /// @pre `hdl != invalid_connection_handle && nid != none`
  void add_direct(const connection_handle& hdl, const node_id& nid);

  /// Adds a new indirect route to the table.
  bool add_indirect(const node_id& hop, const node_id& dest);

  /// Blacklist the route to `dest` via `hop`.
  void blacklist(const node_id& hop, const node_id& dest);

  /// Removes a direct connection and calls `cb` for any node
  /// that became unreachable as a result of this operation,
  /// including the node that is assigned as direct path for `hdl`.
  void erase_direct(const connection_handle& hdl, erase_callback& cb);

  /// Removes any entry for indirect connection to `dest` and returns
  /// `true` if `dest` had an indirect route, otherwise `false`.
  bool erase_indirect(const node_id& dest);

  /// Queries whether `dest` is reachable.
  bool reachable(const node_id& dest);

  /// Removes all direct and indirect routes to `dest` and calls
  /// `cb` for any node that became unreachable as a result of this
  /// operation, including `dest`.
  /// @returns the number of removed routes (direct and indirect)
  size_t erase(const node_id& dest, erase_callback& cb);

  /// Returns the parent broker.
  inline abstract_broker* parent() {
    return parent_;
  }

public:
  template <class Map, class Fallback>
  typename Map::mapped_type
  get_opt(const Map& m, const typename Map::key_type& k, Fallback&& x) const {
    auto i = m.find(k);
    if (i != m.end())
      return i->second;
    return std::forward<Fallback>(x);
  }

  using node_id_set = std::unordered_set<node_id>;

  using indirect_entries = std::unordered_map<node_id,      // dest
                                              node_id_set>; // hop

  abstract_broker* parent_;
  std::unordered_map<connection_handle, node_id> direct_by_hdl_;
  std::unordered_map<node_id, connection_handle> direct_by_nid_;
  indirect_entries indirect_;
  indirect_entries blacklist_;
};

/// @}

} // namespace basp
} // namespace io
} // namespace caf

