// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_addr.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/exit_reason.hpp"
#include "caf/fwd.hpp"
#include "caf/node_id.hpp"

#include <functional>
#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace caf {

/// Groups a (distributed) set of actors and allows actors
/// in the same namespace to exchange messages.
class CAF_CORE_EXPORT proxy_registry {
public:
  /// Responsible for creating proxy actors.
  class CAF_CORE_EXPORT backend {
  public:
    virtual ~backend();

    /// Creates a new proxy instance.
    virtual strong_actor_ptr make_proxy(node_id, actor_id) = 0;

    /// Sets the thread-local last-hop pointer to detect indirect connections.
    virtual void set_last_hop(node_id* ptr) = 0;
  };

  /// Returns the current proxy registry for the calling thread.
  static proxy_registry* current() noexcept;

  /// Sets the current proxy registry for the calling thread.
  static void current(proxy_registry*) noexcept;

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
  size_t count_proxies(const node_id& node) const;

  /// Returns the proxy instance identified by `node` and `aid`.
  strong_actor_ptr get(const node_id& node, actor_id aid) const;

  /// Returns the proxy instance identified by `node` and `aid`
  /// or creates a new (default) proxy instance.
  strong_actor_ptr get_or_put(const node_id& nid, actor_id aid);

  /// Returns all known proxies.
  std::vector<strong_actor_ptr> get_all(const node_id& node) const;

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
  actor_system& system() {
    return system_;
  }

  /// Returns the hosting actor system.
  const actor_system& system() const {
    return system_;
  }

  /// Sets the thread-local last hop variable on the backend.
  void set_last_hop(node_id* ptr) {
    backend_.set_last_hop(ptr);
  }

private:
  /// @pre mtx_ is locked
  void kill_proxy(strong_actor_ptr&, error);

  actor_system& system_;
  backend& backend_;
  mutable std::mutex mtx_;
  std::unordered_map<node_id, proxy_map> proxies_;
};

} // namespace caf
