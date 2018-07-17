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

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "caf/config.hpp"
#include "caf/extend.hpp"
#include "caf/ref_counted.hpp"

#include "caf/io/fwd.hpp"
#include "caf/io/scribe.hpp"
#include "caf/io/doorman.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/io/datagram_handle.hpp"
#include "caf/io/datagram_servant.hpp"
#include "caf/io/connection_handle.hpp"

#include "caf/io/network/rw_state.hpp"
#include "caf/io/network/operation.hpp"
#include "caf/io/network/ip_endpoint.hpp"
#include "caf/io/network/multiplexer.hpp"
#include "caf/io/network/pipe_reader.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/receive_buffer.hpp"
#include "caf/io/network/stream_manager.hpp"
#include "caf/io/network/acceptor_manager.hpp"
#include "caf/io/network/datagram_manager.hpp"

#include "caf/logger.hpp"

// Forward declaration of C types.
extern "C" {

struct pollfd;
struct epoll_event;

} // extern "C"

// Pick a backend for the multiplexer, depending on the settings in config.hpp.
#if !defined(CAF_LINUX) || defined(CAF_POLL_IMPL)
#define CAF_POLL_MULTIPLEXER
#else
#define CAF_EPOLL_MULTIPLEXER
#endif

namespace caf {
namespace io {
namespace network {

// Define type aliases based on backend type.
#ifdef CAF_POLL_MULTIPLEXER

using event_mask_type = short;
using multiplexer_data = pollfd;
using multiplexer_poll_shadow_data = std::vector<event_handler*>;

#else // CAF_POLL_MULTIPLEXER

using event_mask_type = int;
using multiplexer_data = epoll_event;
using multiplexer_poll_shadow_data = native_socket;

#endif // CAF_POLL_MULTIPLEXER

/// Defines the bitmask for input (read) socket events.
extern const event_mask_type input_mask;

/// Defines the bitmask for output (write) socket events.
extern const event_mask_type output_mask;

/// Defines the bitmask for error socket events.
extern const event_mask_type error_mask;

class default_multiplexer : public multiplexer {
public:
  friend class io::middleman; // disambiguate reference
  friend class supervisor;

  struct event {
    native_socket fd;
    int mask;
    event_handler* ptr;
  };

  struct event_less {
    inline bool operator()(native_socket lhs, const event& rhs) const {
      return lhs < rhs.fd;
    }
    inline bool operator()(const event& lhs, native_socket rhs) const {
      return lhs.fd < rhs;
    }
    inline bool operator()(const event& lhs, const event& rhs) const {
      return lhs.fd < rhs.fd;
    }
  };

  scribe_ptr new_scribe(native_socket fd) override;

  expected<scribe_ptr> new_tcp_scribe(const std::string& host,
                                      uint16_t port) override;

  doorman_ptr new_doorman(native_socket fd) override;

  expected<doorman_ptr> new_tcp_doorman(uint16_t port, const char* in,
                                        bool reuse_addr) override;

  datagram_servant_ptr new_datagram_servant(native_socket fd) override;

  datagram_servant_ptr
  new_datagram_servant_for_endpoint(native_socket fd,
                                    const ip_endpoint& ep) override;

  expected<datagram_servant_ptr>
  new_remote_udp_endpoint(const std::string& host, uint16_t port) override;

  expected<datagram_servant_ptr>
  new_local_udp_endpoint(uint16_t port,const char* in = nullptr,
                         bool reuse_addr = false) override;

  void exec_later(resumable* ptr) override;

  explicit default_multiplexer(actor_system* sys);

  ~default_multiplexer() override;

  supervisor_ptr make_supervisor() override;

  /// Tries to run one or more events.
  /// @returns `true` if at least one event occurred, otherwise `false`.
  bool poll_once(bool block);

  bool try_run_once() override;

  void run_once() override;

  void run() override;

  void add(operation op, native_socket fd, event_handler* ptr);

  void del(operation op, native_socket fd, event_handler* ptr);

  /// Calls `ptr->resume`.
  void resume(intrusive_ptr<resumable> ptr);

  /// Get the next id to create a new datagram handle
  int64_t next_endpoint_id();

  /// Returns the number of socket handlers.
  size_t num_socket_handlers() const noexcept;

  /// Run all pending events generated from calls to `add` or `del`.
  void handle_internal_events();

private:
  /// Calls `epoll`, `kqueue`, or `poll` with or without blocking.
  bool poll_once_impl(bool block);

  // platform-dependent additional initialization code
  void init();

  template <class F>
  void new_event(F fun, operation op, native_socket fd, event_handler* ptr) {
    CAF_ASSERT(fd != invalid_native_socket);
    CAF_ASSERT(ptr != nullptr || fd == pipe_.first);
    // the only valid input where ptr == nullptr is our pipe
    // read handle which is only registered for reading
    auto old_bf = ptr ? ptr->eventbf() : input_mask;
    //auto bf = fun(op, old_bf);
    CAF_LOG_TRACE(CAF_ARG(op) << CAF_ARG(fd) << CAF_ARG(old_bf));
    auto last = events_.end();
    auto i = std::lower_bound(events_.begin(), last, fd, event_less{});
    if (i != last && i->fd == fd) {
      CAF_ASSERT(ptr == i->ptr);
      // squash events together
      CAF_LOG_DEBUG("squash events:" << CAF_ARG(i->mask)
                    << CAF_ARG(fun(op, i->mask)));
      auto bf = i->mask;
      i->mask = fun(op, bf);
      if (i->mask == bf) {
        // didn'""t do a thing
        CAF_LOG_DEBUG("squashing did not change the event");
      } else if (i->mask == old_bf) {
        // just turned into a nop
        CAF_LOG_DEBUG("squashing events resulted in a NOP");
        events_.erase(i);
      }
    } else {
      // insert new element
      auto bf = fun(op, old_bf);
      if (bf == old_bf) {
        CAF_LOG_DEBUG("event has no effect (discarded): "
                 << CAF_ARG(bf) << ", " << CAF_ARG(old_bf));
      } else {
        CAF_LOG_DEBUG("added handler:" << CAF_ARG(fd) << CAF_ARG(op));
        events_.insert(i, event{fd, bf, ptr});
      }
    }
  }

  void handle(const event& e);

  void handle_socket_event(native_socket fd, int mask, event_handler* ptr);

  void close_pipe();

  void wr_dispatch_request(resumable* ptr);

  /// Socket handle to an OS-level event loop such as `epoll`. Unused in the
  /// `poll` implementation.
  native_socket epollfd_; // unused in poll() implementation

  /// Platform-dependent bookkeeping data, e.g., `pollfd` or `epoll_event`.
  std::vector<multiplexer_data> pollset_;

  /// Insertion and deletion events. This vector is always sorted by `.fd`.
  std::vector<event> events_;

  /// Platform-dependent meta data for `pollset_`. This allows O(1) lookup of
  /// event handlers from `pollfd`.
  multiplexer_poll_shadow_data shadow_;

  /// Pipe for pushing events and callbacks into the multiplexer's thread.
  std::pair<native_socket, native_socket> pipe_;

  /// Special-purpose event handler for the pipe.
  pipe_reader pipe_reader_;

  /// Events posted from the multiplexer's own thread are cached in this vector
  /// in order to prevent the multiplexer from writing into its own pipe. This
  /// avoids a possible deadlock where the multiplexer is blocked in
  /// `wr_dispatch_request` when the pipe's buffer is full.
  std::vector<intrusive_ptr<resumable>> internally_posted_;

  /// Sequential ids for handles of datagram servants
  int64_t servant_ids_;

  /// Maximum messages per resume run.
  size_t max_throughput_;
};

inline connection_handle conn_hdl_from_socket(native_socket fd) {
  return connection_handle::from_int(int64_from_native_socket(fd));
}

inline accept_handle accept_hdl_from_socket(native_socket fd) {
  return accept_handle::from_int(int64_from_native_socket(fd));
}

expected<native_socket>
new_tcp_connection(const std::string& host, uint16_t port,
                   optional<protocol::network> preferred = none);

expected<native_socket>
new_tcp_acceptor_impl(uint16_t port, const char* addr, bool reuse_addr);

expected<std::pair<native_socket, ip_endpoint>>
new_remote_udp_endpoint_impl(const std::string& host, uint16_t port,
                             optional<protocol::network> preferred = none);

expected<std::pair<native_socket, protocol::network>>
new_local_udp_endpoint_impl(uint16_t port, const char* addr,
                            bool reuse_addr = false,
                            optional<protocol::network> preferred = none);

} // namespace network
} // namespace io
} // namespace caf
