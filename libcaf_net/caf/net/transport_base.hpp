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

#include "caf/actor_system_config.hpp"
#include "caf/byte.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/net/defaults.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/transport_worker.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace net {

template <class NextLayer, class Handle, class Application, class IdType>
class transport_base {
public:
  // -- member types -----------------------------------------------------------

  using next_layer_type = NextLayer;

  using handle_type = Handle;

  using application_type = Application;

  using id_type = IdType;

  using buffer_type = std::vector<byte>;

  using buffer_cache_type = std::vector<buffer_type>;

  // -- constructors, destructors, and assignment operators --------------------

  transport_base(Handle handle, application_type application)
    : next_layer_(std::move(application)),
      handle_(handle),
      // max_consecutive_reads_(0),
      read_threshold_(1024),
      collected_(0),
      max_(1024),
      rd_flag_(net::receive_policy_flag::exactly),
      written_(0),
      manager_(nullptr) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  handle_type handle() const noexcept {
    return handle_;
  }

  actor_system& system() {
    return manager().system();
  }

  application_type& application() {
    return next_layer_.application();
  }

  transport_base& transport() {
    return *this;
  }

  endpoint_manager& manager() {
    return *manager_;
  }

  // -- member functions -------------------------------------------------------

  virtual error init(endpoint_manager& parent) {
    manager_ = &parent;
    auto& cfg = system().config();
    auto max_header_bufs = get_or(cfg, "middleman.max-header-buffers",
                                  defaults::middleman::max_header_buffers);
    header_bufs_.reserve(max_header_bufs);
    auto max_payload_bufs = get_or(cfg, "middleman.max-payload-buffers",
                                   defaults::middleman::max_payload_buffers);
    payload_bufs_.reserve(max_payload_bufs);
    if (auto err = next_layer_.init(*this))
      return err;
    return none;
  }

  virtual bool handle_read_event(endpoint_manager&) = 0;

  virtual bool handle_write_event(endpoint_manager& parent) = 0;

  void resolve(endpoint_manager&, const uri& locator, const actor& listener) {
    next_layer_.resolve(*this, locator.path(), listener);
  }

  void new_proxy(endpoint_manager&, const node_id& peer, actor_id id) {
    next_layer_.new_proxy(*this, peer, id);
  }

  void local_actor_down(endpoint_manager&, const node_id& peer, actor_id id,
                        error reason) {
    next_layer_.local_actor_down(*this, peer, id, std::move(reason));
  }

  void timeout(endpoint_manager&, atom_value value, uint64_t id) {
    next_layer_.timeout(*this, value, id);
  }

  template <class... Ts>
  void set_timeout(uint64_t, Ts&&...) {
    // TODO: implement me!
  }

  void handle_error(sec code) {
    next_layer_.handle_error(code);
  }

  void prepare_next_read() {
    collected_ = 0;
    // This cast does nothing, but prevents a weird compiler error on GCC
    // <= 4.9.
    // TODO: remove cast when dropping support for GCC 4.9.
    switch (static_cast<net::receive_policy_flag>(rd_flag_)) {
      case net::receive_policy_flag::exactly:
        if (read_buf_.size() != max_)
          read_buf_.resize(max_);
        read_threshold_ = max_;
        break;
      case net::receive_policy_flag::at_most:
        if (read_buf_.size() != max_)
          read_buf_.resize(max_);
        read_threshold_ = 1;
        break;
      case net::receive_policy_flag::at_least: {
        // read up to 10% more, but at least allow 100 bytes more
        auto max_size = max_ + std::max<size_t>(100, max_ / 10);
        if (read_buf_.size() != max_size)
          read_buf_.resize(max_size);
        read_threshold_ = max_;
        break;
      }
    }
  }

  void configure_read(receive_policy::config cfg) {
    rd_flag_ = cfg.first;
    max_ = cfg.second;
    prepare_next_read();
  }

  // TODO: make this work for both datagram and stream oriented transports.
  virtual void write_packet(id_type id, span<buffer_type*> buffers) = 0;

  // -- buffer management ------------------------------------------------------

  buffer_type next_header_buffer() {
    return next_buffer_impl(header_bufs_);
  }

  buffer_type next_payload_buffer() {
    return next_buffer_impl(payload_bufs_);
  }

private:
  // -- utility functions ------------------------------------------------------

  static buffer_type next_buffer_impl(buffer_cache_type cache) {
    if (cache.empty()) {
      return {};
    }
    auto buf = std::move(cache.back());
    cache.pop_back();
    return buf;
  }

protected:
  NextLayer next_layer_;
  Handle handle_;

  buffer_cache_type header_bufs_;
  buffer_cache_type payload_bufs_;

  buffer_type read_buf_;
  std::deque<std::pair<bool, buffer_type>> write_queue_;

  // TODO implement retries using this member!
  // size_t max_consecutive_reads_;
  size_t read_threshold_;
  size_t collected_;
  size_t max_;
  receive_policy_flag rd_flag_;

  size_t written_;

  endpoint_manager* manager_;
};

} // namespace net
} // namespace caf
