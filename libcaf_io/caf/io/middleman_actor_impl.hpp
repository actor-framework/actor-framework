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

#include "caf/fwd.hpp"
#include "caf/atom.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

#include "caf/io/fwd.hpp"
#include "caf/io/middleman_actor.hpp"

namespace caf {
namespace io {

/// Default implementation of the `middleman_actor` interface.
class middleman_actor_impl : public middleman_actor::base {
public:
  using put_res = result<uint16_t>;

  using mpi_set = std::set<std::string>;

  using get_res = result<node_id, strong_actor_ptr, mpi_set>;

  using get_delegated = delegated<node_id, strong_actor_ptr, mpi_set>;

  using del_res = result<void>;

  using endpoint_data = std::tuple<node_id, strong_actor_ptr, mpi_set>;

  using endpoint = std::pair<std::string, uint16_t>;

  middleman_actor_impl(actor_config& cfg, actor default_broker);

  void on_exit() override;

  const char* name() const override;

  behavior_type make_behavior() override;

protected:
  /// Tries to connect to given `host` and `port`. The default implementation
  /// calls `system().middleman().backend().new_tcp_scribe(host, port)`.
  virtual expected<scribe_ptr> connect(const std::string& host, uint16_t port);

  /// Tries to connect to given `host` and `port`. The default implementation
  /// calls `system().middleman().backend().new_udp`.
  virtual expected<datagram_servant_ptr> contact(const std::string& host,
                                                 uint16_t port);

  /// Tries to open a local port. The default implementation calls
  /// `system().middleman().backend().new_tcp_doorman(port, addr, reuse)`.
  virtual expected<doorman_ptr> open(uint16_t port, const char* addr,
                                     bool reuse);

  /// Tries to open a local port. The default implementation calls
  /// `system().middleman().backend().new_tcp_doorman(port, addr, reuse)`.
  virtual expected<datagram_servant_ptr> open_udp(uint16_t port, 
                                                  const char* addr, bool reuse);

private:
  put_res put(uint16_t port, strong_actor_ptr& whom, mpi_set& sigs,
              const char* in = nullptr, bool reuse_addr = false);

  put_res put_udp(uint16_t port, strong_actor_ptr& whom, mpi_set& sigs,
                  const char* in = nullptr, bool reuse_addr = false);

  optional<endpoint_data&> cached_tcp(const endpoint& ep);
  optional<endpoint_data&> cached_udp(const endpoint& ep);

  optional<std::vector<response_promise>&> pending(const endpoint& ep);

  actor broker_;
  std::map<endpoint, endpoint_data> cached_tcp_;
  std::map<endpoint, endpoint_data> cached_udp_;
  std::map<endpoint, std::vector<response_promise>> pending_;
};

} // namespace io
} // namespace caf

