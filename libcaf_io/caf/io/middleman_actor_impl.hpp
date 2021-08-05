// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/io_export.hpp"
#include "caf/fwd.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/middleman_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

namespace caf::io {

/// Default implementation of the `middleman_actor` interface.
class CAF_IO_EXPORT middleman_actor_impl : public middleman_actor::base {
public:
  using put_res = result<uint16_t>;

  using mpi_set = std::set<std::string>;

  using get_res = result<node_id, strong_actor_ptr, mpi_set>;

  using get_delegated = delegated<node_id, strong_actor_ptr, mpi_set>;

  using del_res = result<void>;

  using endpoint_data = std::tuple<node_id, strong_actor_ptr, mpi_set>;

  using endpoint = std::pair<std::string, uint16_t>;

  middleman_actor_impl(actor_config& cfg, actor default_broker);

  middleman_actor_impl(middleman_actor_impl&&) = delete;

  middleman_actor_impl(const middleman_actor_impl&) = delete;

  middleman_actor_impl& operator=(middleman_actor_impl&&) = delete;

  middleman_actor_impl& operator=(const middleman_actor_impl&) = delete;

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

  endpoint_data* cached_tcp(const endpoint& ep);
  endpoint_data* cached_udp(const endpoint& ep);

  std::vector<response_promise>* pending(const endpoint& ep);

  actor broker_;
  std::map<endpoint, endpoint_data> cached_tcp_;
  std::map<endpoint, endpoint_data> cached_udp_;
  std::map<endpoint, std::vector<response_promise>> pending_;
};

} // namespace caf::io
