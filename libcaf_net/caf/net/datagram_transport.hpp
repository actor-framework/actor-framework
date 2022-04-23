// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <deque>
#include <vector>

#include "caf/byte_buffer.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/logger.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/transport_base.hpp"
#include "caf/net/transport_worker_dispatcher.hpp"
#include "caf/net/udp_datagram_socket.hpp"
#include "caf/sec.hpp"

namespace caf::net {

template <class Factory>
using datagram_transport_base
  = transport_base<datagram_transport<Factory>,
                   transport_worker_dispatcher<Factory, ip_endpoint>,
                   udp_datagram_socket, Factory, ip_endpoint>;

/// Implements a udp_transport policy that manages a datagram socket.
template <class Factory>
class datagram_transport : public datagram_transport_base<Factory> {
public:
  // Maximal UDP-packet size
  static constexpr size_t max_datagram_size
    = std::numeric_limits<uint16_t>::max();

  // -- member types -----------------------------------------------------------

  using factory_type = Factory;

  using id_type = ip_endpoint;

  using application_type = typename factory_type::application_type;

  using super = datagram_transport_base<factory_type>;

  using buffer_cache_type = typename super::buffer_cache_type;

  // -- constructors, destructors, and assignment operators --------------------

  datagram_transport(udp_datagram_socket handle, factory_type factory)
    : super(handle, std::move(factory)) {
    // nop
  }

  // -- public member functions ------------------------------------------------

  error init(endpoint_manager& manager) override {
    CAF_LOG_TRACE("");
    if (auto err = super::init(manager))
      return err;
    prepare_next_read();
    return none;
  }

  bool handle_read_event(endpoint_manager&) override {
    CAF_LOG_TRACE(CAF_ARG(this->handle_.id));
    for (size_t reads = 0; reads < this->max_consecutive_reads_; ++reads) {
      auto ret = read(this->handle_, this->read_buf_);
      if (auto res = get_if<std::pair<size_t, ip_endpoint>>(&ret)) {
        auto& [num_bytes, ep] = *res;
        CAF_LOG_DEBUG("received " << num_bytes << " bytes");
        this->read_buf_.resize(num_bytes);
        if (auto err = this->next_layer_.handle_data(*this, this->read_buf_,
                                                     std::move(ep))) {
          CAF_LOG_ERROR("handle_data failed: " << err);
          return false;
        }
        prepare_next_read();
      } else {
        auto err = get<sec>(ret);
        if (err == sec::unavailable_or_would_block) {
          break;
        } else {
          CAF_LOG_DEBUG("read failed" << CAF_ARG(err));
          this->next_layer_.handle_error(err);
          return false;
        }
      }
    }
    return true;
  }

  bool handle_write_event(endpoint_manager& manager) override {
    CAF_LOG_TRACE(CAF_ARG2("socket", this->handle_.id)
                  << CAF_ARG2("queue-size", packet_queue_.size()));
    auto fetch_next_message = [&] {
      if (auto msg = manager.next_message()) {
        this->next_layer_.write_message(*this, std::move(msg));
        return true;
      }
      return false;
    };
    do {
      if (auto err = write_some())
        return err == sec::unavailable_or_would_block;
    } while (fetch_next_message());
    return !packet_queue_.empty();
  }

  // TODO: remove this function. `resolve` should add workers when needed.
  error add_new_worker(node_id node, id_type id) {
    auto worker = this->next_layer_.add_new_worker(*this, node, id);
    if (!worker)
      return worker.error();
    return none;
  }

  void write_packet(id_type id, span<byte_buffer*> buffers) override {
    CAF_LOG_TRACE("");
    CAF_ASSERT(!buffers.empty());
    if (packet_queue_.empty())
      this->manager().register_writing();
    // By convention, the first buffer is a header buffer. Every other buffer is
    // a payload buffer.
    packet_queue_.emplace_back(id, buffers);
  }

  /// Helper struct for managing outgoing packets
  struct packet {
    id_type id;
    buffer_cache_type bytes;
    size_t size;

    packet(id_type id, span<byte_buffer*> bufs) : id(id) {
      size = 0;
      for (auto buf : bufs) {
        size += buf->size();
        bytes.emplace_back(std::move(*buf));
      }
    }

    std::vector<byte_buffer*> get_buffer_ptrs() {
      std::vector<byte_buffer*> ptrs;
      for (auto& buf : bytes)
        ptrs.emplace_back(&buf);
      return ptrs;
    }
  };

private:
  // -- utility functions ------------------------------------------------------

  void prepare_next_read() {
    this->read_buf_.resize(max_datagram_size);
  }

  error write_some() {
    // Helper function to sort empty buffers back into the right caches.
    auto recycle = [&]() {
      auto& front = packet_queue_.front();
      auto& bufs = front.bytes;
      auto it = bufs.begin();
      if (this->header_bufs_.size() < this->header_bufs_.capacity()) {
        it->clear();
        this->header_bufs_.emplace_back(std::move(*it++));
      }
      for (; it != bufs.end()
             && this->payload_bufs_.size() < this->payload_bufs_.capacity();
           ++it) {
        it->clear();
        this->payload_bufs_.emplace_back(std::move(*it));
      }
      packet_queue_.pop_front();
    };
    // Write as many bytes as possible.
    while (!packet_queue_.empty()) {
      auto& packet = packet_queue_.front();
      auto ptrs = packet.get_buffer_ptrs();
      auto write_ret = write(this->handle_, ptrs, packet.id);
      if (auto num_bytes = get_if<size_t>(&write_ret)) {
        CAF_LOG_DEBUG(CAF_ARG(this->handle_.id) << CAF_ARG(*num_bytes));
        CAF_LOG_WARNING_IF(*num_bytes < packet.size,
                           "packet was not sent completely");
        recycle();
      } else {
        auto err = get<sec>(write_ret);
        if (err != sec::unavailable_or_would_block) {
          CAF_LOG_ERROR("write failed" << CAF_ARG(err));
          this->next_layer_.handle_error(err);
        }
        return err;
      }
    }
    return none;
  }

  std::deque<packet> packet_queue_;
};

} // namespace caf::net
