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

#include <vector>
#include <unordered_map>

#include "caf/logger.hpp"
#include "caf/raise_error.hpp"
#include "caf/ref_counted.hpp"

#include "caf/io/fwd.hpp"
#include "caf/io/receive_policy.hpp"

#include "caf/io/network/ip_endpoint.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/datagram_manager.hpp"

namespace caf {
namespace io {
namespace network {

class datagram_handler : public event_handler {
public:
  /// A smart pointer to a datagram manager.
  using manager_ptr = intrusive_ptr<datagram_manager>;

  /// A buffer class providing a compatible interface to `std::vector`.
  using write_buffer_type = std::vector<char>;
  using read_buffer_type = network::receive_buffer;

  /// A job for sending a datagram consisting of the sender and a buffer.
  using job_type = std::pair<datagram_handle, write_buffer_type>;

  datagram_handler(default_multiplexer& backend_ref, native_socket sockfd);

  /// Starts reading data from the socket, forwarding incoming data to `mgr`.
  void start(datagram_manager* mgr);

  /// Activates the datagram handler.
  void activate(datagram_manager* mgr);

  void ack_writes(bool x);

  /// Copies data to the write buffer.
  /// @warning Not thread safe.
  void write(datagram_handle hdl, const void* buf, size_t num_bytes);

  /// Returns the write buffer of this enpoint.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  inline write_buffer_type& wr_buf(datagram_handle hdl) {
    wr_offline_buf_.emplace_back();
    wr_offline_buf_.back().first = hdl;
    return wr_offline_buf_.back().second;
  }

  /// Enqueues a buffer to be sent as a datagram.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  inline void enqueue_datagram(datagram_handle hdl, std::vector<char> buf) {
    wr_offline_buf_.emplace_back(hdl, move(buf));
  }

  /// Returns the read buffer of this stream.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  inline read_buffer_type& rd_buf() {
    return rd_buf_;
  }

  /// Sends the content of the write buffer, calling the `io_failure`
  /// member function of `mgr` in case of an error.
  /// @warning Must not be called outside the IO multiplexers event loop
  ///          once the stream has been started.
  void flush(const manager_ptr& mgr);

  /// Closes the read channel of the underlying socket and removes
  /// this handler from its parent.
  void stop_reading();

  void removed_from_loop(operation op) override;

  void add_endpoint(datagram_handle hdl, const ip_endpoint& ep,
                    const manager_ptr mgr);

  std::unordered_map<datagram_handle, ip_endpoint>& endpoints();

  const std::unordered_map<datagram_handle, ip_endpoint>& endpoints() const;

  void remove_endpoint(datagram_handle hdl);

  inline ip_endpoint& sending_endpoint() {
    return sender_;
  }

protected:
  template <class Policy>
  void handle_event_impl(io::network::operation op, Policy& policy) {
    CAF_LOG_TRACE(CAF_ARG(op));
    auto mcr = max_consecutive_reads_;
    switch (op) {
      case io::network::operation::read: {
        // Loop until an error occurs or we have nothing more to read
        // or until we have handled `mcr` reads.
        for (size_t i = 0; i < mcr; ++i) {
          auto res = policy.read_datagram(num_bytes_, fd(), rd_buf_.data(),
                                          rd_buf_.size(), sender_);
          if (!handle_read_result(res))
            return;
        }
        break;
      }
      case io::network::operation::write: {
        size_t wb; // written bytes
        auto itr = ep_by_hdl_.find(wr_buf_.first);
        // maybe this could be an assert?
        if (itr == ep_by_hdl_.end())
          CAF_RAISE_ERROR("got write event for undefined endpoint");
        auto& id = itr->first;
        auto& ep = itr->second;
        std::vector<char> buf;
        std::swap(buf, wr_buf_.second);
        auto size_as_int = static_cast<int>(buf.size());
        if (size_as_int > send_buffer_size_) {
          send_buffer_size_ = size_as_int;
          send_buffer_size(fd(), size_as_int);
        }
        auto res = policy.write_datagram(wb, fd(), buf.data(), buf.size(), ep);
        handle_write_result(res, id, buf, wb);
        break;
      }
      case operation::propagate_error:
        handle_error();
        break;
    }
  }

private:
  size_t max_consecutive_reads_;

  void prepare_next_read();

  void prepare_next_write();

  bool handle_read_result(bool read_result);

  void handle_write_result(bool write_result, datagram_handle id,
                           std::vector<char>& buf, size_t wb);

  void handle_error();

  // known endpoints and broker servants
  std::unordered_map<ip_endpoint, datagram_handle> hdl_by_ep_;
  std::unordered_map<datagram_handle, ip_endpoint> ep_by_hdl_;

  // state for reading
  const size_t max_datagram_size_;
  size_t num_bytes_;
  read_buffer_type rd_buf_;
  manager_ptr reader_;
  ip_endpoint sender_;

  // state for writing
  int send_buffer_size_;
  bool ack_writes_;
  bool writing_;
  std::deque<job_type> wr_offline_buf_;
  job_type wr_buf_;
  manager_ptr writer_;
};

} // namespace network
} // namespace io
} // namespace caf
