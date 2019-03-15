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

#include <utility>
#include <functional>
#include <unordered_map>

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/exit_reason.hpp"

namespace caf {

/// Groups a (distributed) set of actors and allows actors
/// in the same namespace to exchange messages.
class proxy_registry {
public:
  /// Responsible for creating proxy actors.
  class backend {
  public:
    virtual ~backend();

    /// Creates a new proxy instance.
    virtual strong_actor_ptr make_proxy(node_id, actor_id) = 0;

    virtual execution_unit* registry_context() = 0;
  };

  proxy_registry(actor_system& sys, backend& be);

  proxy_registry(const proxy_registry&) = delete;
  proxy_registry& operator=(const proxy_registry&) = delete;

  ~proxy_registry();

  void serialize(serializer& sink, const actor_addr& addr) const;

  void serialize(deserializer& source, actor_addr& addr);

  /// Writes an actor address to `sink` and adds the actor
  /// to the list of known actors for a later deserialization.
  void write(serializer* sink, const actor_addr& ptr) const;

  /// Reads an actor address from `source,` creating
  /// addresses for remote actors on the fly if needed.
  actor_addr read(deserializer* source);

  /// A map that stores all proxies for known remote actors.
  using proxy_map = std::map<actor_id, strong_actor_ptr>;

  /// Returns the number of proxies for `node`.
  size_t count_proxies(const node_id& node);

  /// Returns the proxy instance identified by `node` and `aid`.
  strong_actor_ptr get(const node_id& node, actor_id aid);

  /// Returns the proxy instance identified by `node` and `aid`
  /// or creates a new (default) proxy instance.
  strong_actor_ptr get_or_put(const node_id& nid, actor_id aid);

  /// Returns all known proxies.
  std::vector<strong_actor_ptr> get_all(const node_id& node);

  /// Deletes all proxies for `node`.
  void erase(const node_id& nid);

  /// Deletes the proxy with id `aid` for `nid`.
  void erase(const node_id& nid, actor_id aid,
             error rsn = exit_reason::remote_link_unreachable);

  /// Queries whether there are any proxies left.
  bool empty() const;

  /// Deletes all proxies.
  void clear();

  /// Returns the hosting actor system.
  inline actor_system& system() {
    return system_;
  }

  /// Returns the hosting actor system.
  inline const actor_system& system() const {
    return system_;
  }

  inline size_t size() const {
    return proxies_.size();
  }

private:
  void kill_proxy(strong_actor_ptr&, error);

  actor_system& system_;
  backend& backend_;
  std::unordered_map<node_id, proxy_map> proxies_;
};

} // namespace caf

