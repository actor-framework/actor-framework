/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_IO_NETWORK_TEST_MULTIPLEXER_HPP
#define CAF_IO_NETWORK_TEST_MULTIPLEXER_HPP

#include <thread>

#include "caf/io/receive_policy.hpp"
#include "caf/io/abstract_broker.hpp"

#include "caf/io/network/multiplexer.hpp"

namespace caf {
namespace io {
namespace network {

class test_multiplexer : public multiplexer {
public:
  explicit test_multiplexer(actor_system* sys);

  ~test_multiplexer() override;

  scribe_ptr new_scribe(native_socket) override;

  expected<scribe_ptr> new_tcp_scribe(const std::string& host,
                                      uint16_t port_hint) override;

  doorman_ptr new_doorman(native_socket) override;

  expected<doorman_ptr> new_tcp_doorman(uint16_t prt, const char* in,
                                        bool reuse_addr) override;

  /// Checks whether `x` is assigned to any known doorman or is user-provided
  /// for future assignment.
  bool is_known_port(uint16_t x) const;

  /// Checks whether `x` is assigned to any known doorman or is user-provided
  /// for future assignment.
  bool is_known_handle(accept_handle x) const;

  supervisor_ptr make_supervisor() override;

  bool try_run_once() override;

  void run_once() override;

  void run() override;

  scribe_ptr new_scribe(connection_handle);

  doorman_ptr new_doorman(accept_handle, uint16_t port);

  void provide_scribe(std::string host, uint16_t desired_port,
                      connection_handle hdl);

  void provide_acceptor(uint16_t desired_port, accept_handle hdl);

  /// A buffer storing bytes.
  using buffer_type = std::vector<char>;

  using shared_buffer_type = std::shared_ptr<buffer_type>;

  /// Models pending data on the network, i.e., the network
  /// input buffer usually managed by the operating system.
  buffer_type& virtual_network_buffer(connection_handle hdl);

  /// Returns the output buffer of the scribe identified by `hdl`.
  buffer_type& output_buffer(connection_handle hdl);

  /// Returns the input buffer of the scribe identified by `hdl`.
  buffer_type& input_buffer(connection_handle hdl);

  /// Returns the configured read policy of the scribe identified by `hdl`.
  receive_policy::config& read_config(connection_handle hdl);

  /// Returns whether the scribe identified by `hdl` receives write ACKs.
  bool& ack_writes(connection_handle hdl);

  /// Returns `true` if this handle has been closed
  /// for reading, `false` otherwise.
  bool& stopped_reading(connection_handle hdl);

  /// Returns `true` if this handle is inactive, otherwise `false`.
  bool& passive_mode(connection_handle hdl);

  scribe_ptr& impl_ptr(connection_handle hdl);

  uint16_t& port(accept_handle hdl);

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

  using pending_connects_map = std::unordered_multimap<accept_handle,
                                                       connection_handle>;

  pending_connects_map& pending_connects();

  using pending_scribes_map = std::map<std::pair<std::string, uint16_t>,
                                       connection_handle>;

  using pending_doorman_map = std::unordered_map<uint16_t, accept_handle>;

  bool has_pending_scribe(std::string x, uint16_t y);

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

  /// Appends `buf` to the virtual network buffer of `hdl`
  /// and calls `read_data(hdl)` afterwards.
  void virtual_send(connection_handle hdl, const buffer_type& buf);

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
  void exec_later(resumable* ptr, bool high_prio = true) override;
  bool is_neighbor(execution_unit*) const override;

private:
  using resumable_ptr = intrusive_ptr<resumable>;

  void exec(resumable_ptr& ptr);

  using guard_type = std::unique_lock<std::mutex>;

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

  using scribe_data_map = std::unordered_map<connection_handle, scribe_data>;

  using doorman_data_map = std::unordered_map<accept_handle, doorman_data>;

  // guards resumables_ and scribes_
  std::mutex mx_;
  std::condition_variable cv_;
  std::list<resumable_ptr> resumables_;
  pending_scribes_map scribes_;
  std::unordered_map<uint16_t, accept_handle> doormen_;
  scribe_data_map scribe_data_;
  doorman_data_map doorman_data_;
  pending_connects_map pending_connects_;

  // extra state for making sure the test multiplexer is not used in a
  // multithreaded setup
  std::thread::id tid_;

  // Configures shortcuts for runnables.
  size_t inline_runnables_;

  // Configures a one-shot handler for the next inlined runnable.
  std::function<void()> inline_runnable_callback_;
};

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_TEST_MULTIPLEXER_HPP
