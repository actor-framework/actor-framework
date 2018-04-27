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

#include <thread>

#include "caf/io/receive_policy.hpp"
#include "caf/io/abstract_broker.hpp"

#include "caf/io/network/multiplexer.hpp"
#include "caf/io/network/receive_buffer.hpp"

namespace caf {
namespace io {
namespace network {

class test_multiplexer : public multiplexer {
private:
  struct datagram_data;

public:
  explicit test_multiplexer(actor_system* sys);

  ~test_multiplexer() override;

  scribe_ptr new_scribe(native_socket) override;

  expected<scribe_ptr> new_tcp_scribe(const std::string& host,
                                      uint16_t port_hint) override;

  doorman_ptr new_doorman(native_socket) override;

  expected<doorman_ptr> new_tcp_doorman(uint16_t prt, const char* in,
                                        bool reuse_addr) override;

  datagram_servant_ptr new_datagram_servant(native_socket fd) override;

  datagram_servant_ptr
  new_datagram_servant_for_endpoint(native_socket fd,
                                    const ip_endpoint& ep) override;

  expected<datagram_servant_ptr>
  new_remote_udp_endpoint(const std::string& host, uint16_t port) override;

  expected<datagram_servant_ptr>
  new_local_udp_endpoint(uint16_t port, const char* in = nullptr,
                         bool reuse_addr = false) override;

  /// Checks whether `x` is assigned to any known doorman or is user-provided
  /// for future assignment.
  bool is_known_port(uint16_t x) const;

  /// Checks whether `x` is assigned to any known doorman or is user-provided
  /// for future assignment.
  bool is_known_handle(accept_handle x) const;

  bool is_known_handle(datagram_handle x) const;

  supervisor_ptr make_supervisor() override;

  bool try_run_once() override;

  void run_once() override;

  void run() override;

  scribe_ptr new_scribe(connection_handle);

  doorman_ptr new_doorman(accept_handle, uint16_t port);

public:
  datagram_servant_ptr new_datagram_servant(datagram_handle, uint16_t port);

  datagram_servant_ptr new_datagram_servant(datagram_handle,
                                            const std::string& host,
                                            uint16_t port);

  void provide_scribe(std::string host, uint16_t desired_port,
                      connection_handle hdl);

  void provide_acceptor(uint16_t desired_port, accept_handle hdl);

  void provide_datagram_servant(uint16_t desired_port, datagram_handle hdl);

  void provide_datagram_servant(std::string host, uint16_t desired_port,
                                datagram_handle hdl);

  /// Generate an id for a new servant.
  int64_t next_endpoint_id();

  /// A buffer storing bytes used for TCP related components.
  using buffer_type = std::vector<char>;

  /// Buffers storing bytes for UDP related components.
  using read_buffer_type = network::receive_buffer;
  using write_buffer_type = buffer_type;
  using read_job_type = std::pair<datagram_handle, read_buffer_type>;
  using write_job_type = std::pair<datagram_handle, write_buffer_type>;
  using write_job_queue_type = std::deque<write_job_type>;

  using shared_buffer_type = std::shared_ptr<buffer_type>;
  using shared_job_queue_type = std::shared_ptr<write_job_queue_type>;

  /// Models pending data on the network, i.e., the network
  /// input buffer usually managed by the operating system.
  buffer_type& virtual_network_buffer(connection_handle hdl);

  /// Models pending data on the network, i.e., the network
  /// input buffer usually managed by the operating system.
  write_job_queue_type& virtual_network_buffer(datagram_handle hdl);

  /// Returns the output buffer of the scribe identified by `hdl`.
  buffer_type& output_buffer(connection_handle hdl);

  /// Returns the input buffer of the scribe identified by `hdl`.
  buffer_type& input_buffer(connection_handle hdl);

  /// Returns the output buffer of the dgram servant identified by `hdl`.
  write_job_type& output_buffer(datagram_handle hdl);

  /// Returns the queue with all outgoing datagrams for the dgram servant
  /// identified by `hdl`.
  write_job_queue_type& output_queue(datagram_handle hdl);

  /// Returns the input buffer of the dgram servant identified by `hdl`.
  read_job_type& input_buffer(datagram_handle hdl);

  /// Returns the configured read policy of the scribe identified by `hdl`.
  receive_policy::config& read_config(connection_handle hdl);

  /// Returns whether the scribe identified by `hdl` receives write ACKs.
  bool& ack_writes(connection_handle hdl);

  /// Returns whether the dgram servant identified by `hdl` receives write ACKs.
  bool& ack_writes(datagram_handle hdl);

  /// Returns `true` if this handle has been closed
  /// for reading, `false` otherwise.
  bool& stopped_reading(connection_handle hdl);

  /// Returns `true` if this handle has been closed
  /// for reading, `false` otherwise.
  bool& stopped_reading(datagram_handle hdl);

  /// Returns `true` if this handle is inactive, otherwise `false`.
  bool& passive_mode(connection_handle hdl);

  /// Returns `true` if this handle is inactive, otherwise `false`.
  bool& passive_mode(datagram_handle hdl);

  scribe_ptr& impl_ptr(connection_handle hdl);

  uint16_t& port(accept_handle hdl);

  uint16_t& port(datagram_handle hdl);

  uint16_t& local_port(datagram_handle hdl);

  size_t& datagram_size(datagram_handle hdl);

  datagram_servant_ptr& impl_ptr(datagram_handle hdl);

  /// Returns a map with all servants related to the servant `hdl`.
  std::set<datagram_handle>& servants(datagram_handle hdl);

  /// Returns `true` if this handle has been closed
  /// for reading, `false` otherwise.
  bool& stopped_reading(accept_handle hdl);

  /// Returns `true` if this handle is inactive, otherwise `false`.
  bool& passive_mode(accept_handle hdl);

  doorman_ptr& impl_ptr(accept_handle hdl);

  /// Stores `hdl` as a pending connection for `src`.
  void add_pending_connect(accept_handle src, connection_handle hdl);

  /// Add `hdl` as a pending connect to `src` and provide a scribe on `peer`
  /// that connects the buffers of `hdl` and `peer_hdl`. Calls
  /// `add_pending_connect(...)` and `peer.provide_scribe(...)`.
  void prepare_connection(accept_handle src, connection_handle hdl,
                          test_multiplexer& peer, std::string host,
                          uint16_t port, connection_handle peer_hdl);

  /// Stores `hdl` as a pending endpoint for `src`.
  void add_pending_endpoint(datagram_handle src, datagram_handle hdl);

  using pending_connects_map = std::unordered_multimap<accept_handle,
                                                       connection_handle>;

  pending_connects_map& pending_connects();

  using pending_endpoints_map = std::unordered_map<int64_t, datagram_handle>;

  pending_endpoints_map& pending_endpoints();

  using pending_scribes_map = std::map<std::pair<std::string, uint16_t>,
                                       connection_handle>;

  using pending_doorman_map = std::unordered_map<uint16_t, accept_handle>;

  using pending_local_datagram_endpoints_map = std::map<uint16_t,
                                                        datagram_handle>;

  using pending_remote_datagram_endpoints_map
    = std::map<std::pair<std::string, uint16_t>, datagram_handle>;

  bool has_pending_scribe(std::string x, uint16_t y);

  bool has_pending_remote_endpoint(std::string x, uint16_t y);

  /// Accepts a pending connect on `hdl`.
  void accept_connection(accept_handle hdl);

  /// Tries to accept a pending connection.
  bool try_accept_connection();

  /// Tries to read data on any available scribe.
  bool try_read_data();

  /// Tries to read data from the external input buffer of `hdl`.
  bool try_read_data(connection_handle hdl);

  /// Poll data on all scribes.
  bool read_data();

  /// Reads data from the external input buffer until
  /// the configured read policy no longer allows receiving.
  bool read_data(connection_handle hdl);

  /// Reads the next datagram from the external input buffer.
  bool read_data(datagram_handle hdl);

  /// Appends `buf` to the virtual network buffer of `hdl`
  /// and calls `read_data(hdl)` afterwards.
  void virtual_send(connection_handle hdl, const buffer_type& buf);

  /// Appends `buf` to the virtual network buffer of `hdl`
  /// and calls `read_data(hdl)` afterwards.
  void virtual_send(datagram_handle src, datagram_handle ep,
                    const buffer_type&);

  /// Waits until a `runnable` is available and executes it.
  void exec_runnable();

  /// Returns `true` if a `runnable` was available, `false` otherwise.
  bool try_exec_runnable();

  /// Executes all pending `runnable` objects.
  void flush_runnables();

  /// Executes the next `num` enqueued runnables immediately.
  inline void inline_next_runnables(size_t num) {
    inline_runnables_ += num;
  }

  /// Executes the next enqueued runnable immediately.
  inline void inline_next_runnable() {
    inline_next_runnables(1);
  }

  /// Resets the counter for the next inlined runnables.
  inline void reset_inlining() {
    inline_runnables_ = 0;
  }

  /// Installs a callback that is triggered on the next inlined runnable.
  inline void after_next_inlined_runnable(std::function<void()> f) {
    inline_runnable_callback_ = std::move(f);
  }

protected:
  void exec_later(resumable* ptr) override;

private:
  using resumable_ptr = intrusive_ptr<resumable>;

  void exec(resumable_ptr& ptr);

  using guard_type = std::unique_lock<std::mutex>;

  std::shared_ptr<datagram_data> data_for_hdl(datagram_handle hdl);

  struct scribe_data {
    shared_buffer_type vn_buf_ptr;
    shared_buffer_type wr_buf_ptr;
    buffer_type& vn_buf;
    buffer_type rd_buf;
    buffer_type& wr_buf;
    receive_policy::config recv_conf;
    bool stopped_reading;
    bool passive_mode;
    intrusive_ptr<scribe> ptr;
    bool ack_writes;

    // Allows creating an entangled scribes where the input of this scribe is
    // the output of another scribe and vice versa.
    scribe_data(shared_buffer_type input = std::make_shared<buffer_type>(),
                shared_buffer_type output = std::make_shared<buffer_type>());
  };

  struct doorman_data {
    doorman_ptr ptr;
    uint16_t port;
    bool stopped_reading;
    bool passive_mode;
    doorman_data();
  };

  struct datagram_data {
    shared_job_queue_type vn_buf_ptr;
    shared_job_queue_type wr_buf_ptr;
    write_job_queue_type& vn_buf;
    write_job_queue_type& wr_buf;
    read_job_type rd_buf;
    datagram_servant_ptr ptr;
    bool stopped_reading;
    bool passive_mode;
    bool ack_writes;
    uint16_t port;
    uint16_t local_port;
    std::set<datagram_handle> servants;
    size_t datagram_size;

    // Allows creating an entangled scribes where the input of this scribe is
    // the output of another scribe and vice versa.
    datagram_data(
      shared_job_queue_type input = std::make_shared<write_job_queue_type>(),
      shared_job_queue_type output = std::make_shared<write_job_queue_type>()
    );
  };

  using scribe_data_map = std::unordered_map<connection_handle, scribe_data>;

  using doorman_data_map = std::unordered_map<accept_handle, doorman_data>;

  using datagram_data_map = std::unordered_map<datagram_handle,
                                               std::shared_ptr<datagram_data>>;

  // guards resumables_ and scribes_
  std::mutex mx_;
  std::condition_variable cv_;
  std::list<resumable_ptr> resumables_;
  pending_scribes_map scribes_;
  pending_doorman_map doormen_;
  scribe_data_map scribe_data_;
  doorman_data_map doorman_data_;
  pending_local_datagram_endpoints_map local_endpoints_;
  pending_remote_datagram_endpoints_map remote_endpoints_;
  pending_connects_map pending_connects_;
  pending_endpoints_map pending_endpoints_;
  datagram_data_map datagram_data_;

  // Configures shortcuts for runnables.
  size_t inline_runnables_;

  // Configures a one-shot handler for the next inlined runnable.
  std::function<void()> inline_runnable_callback_;

  int64_t servant_ids_;
};

} // namespace network
} // namespace io
} // namespace caf

