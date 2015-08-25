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

#ifndef CAF_IO_NETWORK_TEST_MULTIPLEXER_HPP
#define CAF_IO_NETWORK_TEST_MULTIPLEXER_HPP

#include "caf/io/receive_policy.hpp"
#include "caf/io/abstract_broker.hpp"

#include "caf/io/network/multiplexer.hpp"

namespace caf {
namespace io {
namespace network {

class test_multiplexer : public multiplexer {
public:
  ~test_multiplexer();

  connection_handle new_tcp_scribe(const std::string& host,
                                   uint16_t port) override;

  void assign_tcp_scribe(abstract_broker* ptr, connection_handle hdl) override;

  connection_handle add_tcp_scribe(abstract_broker*, native_socket) override;

  connection_handle add_tcp_scribe(abstract_broker* ptr,
                                   const std::string& host,
                                   uint16_t port) override;

  std::pair<accept_handle, uint16_t>
  new_tcp_doorman(uint16_t port, const char*, bool) override;

  void assign_tcp_doorman(abstract_broker* ptr, accept_handle hdl) override;

  accept_handle add_tcp_doorman(abstract_broker*, native_socket) override;

  std::pair<accept_handle, uint16_t>
  add_tcp_doorman(abstract_broker* ptr, uint16_t prt,
                  const char* in, bool reuse_addr) override;

  supervisor_ptr make_supervisor() override;

  void run() override;

  void provide_scribe(std::string host, uint16_t port, connection_handle hdl);

  void provide_acceptor(uint16_t port, accept_handle hdl);

  /// A buffer storing bytes.
  using buffer_type = std::vector<char>;

  /// Models pending data on the network, i.e., the network
  /// input buffer usually managed by the operating system.
  buffer_type& virtual_network_buffer(connection_handle hdl);

  /// Returns the output buffer of the scribe identified by `hdl`.
  buffer_type& output_buffer(connection_handle hdl);

  /// Returns the input buffer of the scribe identified by `hdl`.
  buffer_type& input_buffer(connection_handle hdl);

  receive_policy::config& read_config(connection_handle hdl);

  /// Returns `true` if this handle has been closed
  /// for reading, `false` otherwise.
  bool& stopped_reading(connection_handle hdl);

  intrusive_ptr<scribe>& impl_ptr(connection_handle hdl);

  uint16_t& port(accept_handle hdl);

  /// Returns `true` if this handle has been closed
  /// for reading, `false` otherwise.
  bool& stopped_reading(accept_handle hdl);

  intrusive_ptr<doorman>& impl_ptr(accept_handle hdl);

  /// Stores `hdl` as a pending connection for `src`.
  void add_pending_connect(accept_handle src, connection_handle hdl);

  using pending_connects_map = std::unordered_multimap<accept_handle,
                                                       connection_handle>;

  pending_connects_map& pending_connects();

  using pending_scribes_map = std::map<std::pair<std::string, uint16_t>,
                                       connection_handle>;

  pending_scribes_map& pending_scribes();

  /// Accepts a pending connect on `hdl`.
  void accept_connection(accept_handle hdl);

  /// Reads data from the external input buffer until
  /// the configured read policy no longer allows receiving.
  void read_data(connection_handle hdl);

  /// Appends `buf` to the virtual network buffer of `hdl`
  /// and calls `read_data(hdl)` afterwards.
  void virtual_send(connection_handle hdl, const buffer_type& buf);

  /// Waits until a `runnable` is available and executes it.
  void exec_runnable();

  /// Returns `true` if a `runnable` was available, `false` otherwise.
  bool try_exec_runnable();

  /// Executes all pending `runnable` objects.
  void flush_runnables();

protected:
  void dispatch_runnable(runnable_ptr ptr) override;

private:
  using guard_type = std::unique_lock<std::mutex>;

  struct scribe_data {
    buffer_type xbuf;
    buffer_type rd_buf;
    buffer_type wr_buf;
    receive_policy::config recv_conf;
    bool stopped_reading = false;
    intrusive_ptr<scribe> ptr;
  };

  struct doorman_data {
    uint16_t port;
    bool stopped_reading = false;
    intrusive_ptr<doorman> ptr;
  };

  std::mutex mx_;
  std::condition_variable cv_;
  std::list<runnable_ptr> runnables_;
  pending_scribes_map scribes_;
  std::unordered_map<uint16_t, accept_handle> doormen_;
  std::unordered_map<connection_handle, scribe_data> scribe_data_;
  std::unordered_map<accept_handle, doorman_data> doorman_data_;
  pending_connects_map pending_connects_;
};

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_TEST_MULTIPLEXER_HPP
