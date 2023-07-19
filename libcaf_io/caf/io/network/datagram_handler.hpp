// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/fwd.hpp"
#include "caf/io/network/datagram_manager.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/ip_endpoint.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/receive_policy.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/logger.hpp"
#include "caf/raise_error.hpp"
#include "caf/ref_counted.hpp"

#include <unordered_map>
#include <vector>

namespace caf::io::network {

class CAF_IO_EXPORT datagram_handler : public event_handler {
public:
  /// A smart pointer to a datagram manager.
  using manager_ptr = intrusive_ptr<datagram_manager>;

  using read_buffer_type = network::receive_buffer;

  /// A job for sending a datagram consisting of the sender and a buffer.
  using job_type = std::pair<datagram_handle, byte_buffer>;

  datagram_handler(default_multiplexer& backend_ref, native_socket sockfd);

  /// Starts reading data from the socket, forwarding incoming data to `mgr`.
  void start(datagram_manager* mgr);

  /// Activates the datagram handler.
  void activate(datagram_manager* mgr);

  /// Copies data to the write buffer.
  /// @warning Not thread safe.
  void write(datagram_handle hdl, const void* buf, size_t num_bytes);

  /// Returns the write buffer of this endpoint.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  byte_buffer& wr_buf(datagram_handle hdl) {
    wr_offline_buf_.emplace_back();
    wr_offline_buf_.back().first = hdl;
    return wr_offline_buf_.back().second;
  }

  /// Enqueues a buffer to be sent as a datagram.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  void enqueue_datagram(datagram_handle hdl, byte_buffer buf) {
    wr_offline_buf_.emplace_back(hdl, std::move(buf));
  }

  /// Returns the read buffer of this stream.
  /// @warning Must not be modified outside the IO multiplexers event loop
  ///          once the stream has been started.
  read_buffer_type& rd_buf() {
    return rd_buf_;
  }

  /// Sends the content of the write buffer, calling the `io_failure`
  /// member function of `mgr` in case of an error.
  /// @warning Must not be called outside the IO multiplexers event loop
  ///          once the stream has been started.
  void flush(const manager_ptr& mgr);

  /// Return the remote address for a given `hdl`.
  std::string addr(datagram_handle hdl) const;

  void removed_from_loop(operation op) override;

  void graceful_shutdown() override;

  void add_endpoint(datagram_handle hdl, const ip_endpoint& ep,
                    const manager_ptr mgr);

  std::unordered_map<datagram_handle, ip_endpoint>& endpoints();

  const std::unordered_map<datagram_handle, ip_endpoint>& endpoints() const;

  void remove_endpoint(datagram_handle hdl);

  ip_endpoint& sending_endpoint() {
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
        byte_buffer buf;
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
                           byte_buffer& buf, size_t wb);

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
  std::deque<job_type> wr_offline_buf_;
  job_type wr_buf_;
  manager_ptr writer_;
};

} // namespace caf::io::network
