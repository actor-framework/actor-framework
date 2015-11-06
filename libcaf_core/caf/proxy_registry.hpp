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

#ifndef CAF_REMOTE_ACTOR_REGISTRY_HPP
#define CAF_REMOTE_ACTOR_REGISTRY_HPP

#include <utility>
#include <functional>
#include <unordered_map>

#include "caf/fwd.hpp"
#include "caf/node_id.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/exit_reason.hpp"

namespace caf {

/// Groups a (distributed) set of actors and allows actors
/// in the same namespace to exchange messages.
class proxy_registry {
public:
  using key_type = node_id;

  /// Responsible for creating proxy actors.
  class backend {
  public:
    virtual ~backend();

    /// Creates a new proxy instance.
    virtual actor_proxy_ptr make_proxy(key_type, actor_id) = 0;

    virtual execution_unit* registry_context() = 0;
  };

  proxy_registry(actor_system& sys, backend& mgm);

  proxy_registry(const proxy_registry&) = delete;
  proxy_registry& operator=(const proxy_registry&) = delete;

  void serialize(serializer& sink, const actor_addr& addr) const;

  void serialize(deserializer& source, actor_addr& addr);

  /// Writes an actor address to `sink` and adds the actor
  /// to the list of known actors for a later deserialization.
  void write(serializer* sink, const actor_addr& ptr) const;

  /// Reads an actor address from `source,` creating
  /// addresses for remote actors on the fly if needed.
  actor_addr read(deserializer* source);

  /// Ensures that `kill_proxy` is called for each proxy instance.
  class proxy_entry {
  public:
    proxy_entry();
    proxy_entry(actor_proxy::anchor_ptr ptr, backend& ctx);
    proxy_entry(proxy_entry&&) = default;
    proxy_entry& operator=(proxy_entry&&) = default;
    ~proxy_entry();

    void reset(uint32_t rsn);

    void assign(actor_proxy::anchor_ptr ptr, backend& ctx);

    inline explicit operator bool() const {
      return static_cast<bool>(ptr_);
    }

    inline actor_proxy::anchor* operator->() const {
      return ptr_.get();
    }

  private:
    actor_proxy::anchor_ptr ptr_;
    backend* backend_;
  };

  /// A map that stores all proxies for known remote actors.
  using proxy_map = std::map<actor_id, proxy_entry>;

  /// Returns the number of proxies for `node`.
  size_t count_proxies(const key_type& node);

  /// Returns all proxies for `node`.
  std::vector<actor_proxy_ptr> get_all() const;

  /// Returns all proxies for `node`.
  std::vector<actor_proxy_ptr> get_all(const key_type& node) const;

  /// Returns the proxy instance identified by `node` and `aid`
  /// or `nullptr` if the actor either unknown or expired.
  actor_proxy_ptr get(const key_type& node, actor_id aid);

  /// Returns the proxy instance identified by `node` and `aid`
  /// or creates a new (default) proxy instance.
  actor_proxy_ptr get_or_put(const key_type& node, actor_id aid);

  /// Deletes all proxies for `node`.
  void erase(const key_type& node);

  /// Deletes the proxy with id `aid` for `node`.
  void erase(const key_type& node, actor_id aid,
             uint32_t rsn = exit_reason::remote_link_unreachable);

  /// Queries whether there are any proxies left.
  bool empty() const;

  /// Deletes all proxies.
  void clear();

  inline actor_system& system() {
    return system_;
  }

private:
  actor_system& system_;
  backend& backend_;
  std::unordered_map<node_id, proxy_map> proxies_;
};

} // namespace caf

#endif
