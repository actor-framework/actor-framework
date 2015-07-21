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

#ifndef CAF_IO_ABSTRACT_BROKER_HPP
#define CAF_IO_ABSTRACT_BROKER_HPP

#include <map>
#include <vector>

#include "caf/detail/intrusive_partitioned_list.hpp"

#include "caf/io/fwd.hpp"
#include "caf/local_actor.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/connection_handle.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/stream_manager.hpp"
#include "caf/io/network/acceptor_manager.hpp"

namespace caf {
namespace io {

class middleman;

class abstract_broker : public local_actor {
public:
  using buffer_type = std::vector<char>;

  class continuation;

  void enqueue(const actor_addr&, message_id,
               message, execution_unit*) override;

  void enqueue(mailbox_element_ptr, execution_unit*) override;

  void launch(execution_unit* eu, bool lazy, bool hide);

  void cleanup(uint32_t reason);

  /// Manages a low-level IO device for the `broker`.
  class servant {
  public:
    friend class abstract_broker;

    void set_broker(abstract_broker* ptr);

    virtual ~servant();

  protected:
    virtual void remove_from_broker() = 0;

    virtual message disconnect_message() = 0;

    inline abstract_broker* parent() {
      return broker_;
    }

    servant(abstract_broker* ptr);

    void disconnect(bool invoke_disconnect_message);

    bool disconnected_;

    abstract_broker* broker_;
  };

  /// Manages a stream.
  class scribe : public network::stream_manager, public servant {
  public:
    scribe(abstract_broker* parent, connection_handle hdl);

    ~scribe();

    /// Implicitly starts the read loop on first call.
    virtual void configure_read(receive_policy::config config) = 0;

    /// Grants access to the output buffer.
    virtual buffer_type& wr_buf() = 0;

    /// Flushes the output buffer, i.e., sends the content of
    ///    the buffer via the network.
    virtual void flush() = 0;

    inline connection_handle hdl() const {
      return hdl_;
    }

    void io_failure(network::operation op) override;

  protected:
    virtual buffer_type& rd_buf() = 0;

    inline new_data_msg& read_msg() {
      return read_msg_.get_as_mutable<new_data_msg>(0);
    }

    inline const new_data_msg& read_msg() const {
      return read_msg_.get_as<new_data_msg>(0);
    }

    void remove_from_broker() override;

    message disconnect_message() override;

    void consume(const void* data, size_t num_bytes) override;

    connection_handle hdl_;

    message read_msg_;
  };

  using scribe_pointer = intrusive_ptr<scribe>;

  /// Manages incoming connections.
  class doorman : public network::acceptor_manager, public servant {
  public:
    doorman(abstract_broker* parent, accept_handle hdl);

    ~doorman();

    inline accept_handle hdl() const {
      return hdl_;
    }

    void io_failure(network::operation op) override;

    // needs to be launched explicitly
    virtual void launch() = 0;

  protected:
    void remove_from_broker() override;

    message disconnect_message() override;

    inline new_connection_msg& accept_msg() {
      return accept_msg_.get_as_mutable<new_connection_msg>(0);
    }

    inline const new_connection_msg& accept_msg() const {
      return accept_msg_.get_as<new_connection_msg>(0);
    }

    accept_handle hdl_;

    message accept_msg_;
  };

  using doorman_pointer = intrusive_ptr<doorman>;

  // a broker needs friends
  friend class scribe;
  friend class doorman;
  friend class continuation;

  virtual ~abstract_broker();

  /// Modifies the receive policy for given connection.
  /// @param hdl Identifies the affected connection.
  /// @param config Contains the new receive policy.
  void configure_read(connection_handle hdl, receive_policy::config config);

  /// Returns the write buffer for given connection.
  buffer_type& wr_buf(connection_handle hdl);

  /// Writes `data` into the buffer for given connection.
  void write(connection_handle hdl, size_t data_size, const void* data);

  /// Sends the content of the buffer for given connection.
  void flush(connection_handle hdl);

  /// Returns the number of open connections.
  inline size_t num_connections() const {
    return scribes_.size();
  }

  std::vector<connection_handle> connections() const;

  /// @cond PRIVATE

  inline void add_scribe(const scribe_pointer& ptr) {
    scribes_.emplace(ptr->hdl(), ptr);
  }

  connection_handle add_tcp_scribe(const std::string& host, uint16_t port);

  void assign_tcp_scribe(connection_handle hdl);

  connection_handle add_tcp_scribe(network::native_socket fd);

  inline void add_doorman(const doorman_pointer& ptr) {
    doormen_.emplace(ptr->hdl(), ptr);
    if (is_initialized()) {
      ptr->launch();
    }
  }

  std::pair<accept_handle, uint16_t> add_tcp_doorman(uint16_t port = 0,
                                                     const char* in = nullptr,
                                                     bool reuse_addr = false);

  void assign_tcp_doorman(accept_handle hdl);

  accept_handle add_tcp_doorman(network::native_socket fd);

  void invoke_message(mailbox_element_ptr& msg);

  void invoke_message(const actor_addr& sender,
                      message_id mid, message& msg);

  /// Closes all connections and acceptors.
  void close_all();

  /// Closes the connection identified by `handle`.
  /// Unwritten data will still be send.
  void close(connection_handle handle);

  /// Closes the acceptor identified by `handle`.
  void close(accept_handle handle);

  /// Checks whether a connection for `handle` exists.
  bool valid(connection_handle handle);

  /// Checks whether an acceptor for `handle` exists.
  bool valid(accept_handle handle);

protected:
  abstract_broker();

  abstract_broker(middleman& parent_ref);

  /// @endcond

  inline middleman& parent() {
    return mm_;
  }

  network::multiplexer& backend();

  template <class Handle, class T>
  static T& by_id(Handle hdl, std::map<Handle, intrusive_ptr<T>>& elements) {
    auto i = elements.find(hdl);
    if (i == elements.end()) {
      throw std::invalid_argument("invalid handle");
    }
    return *(i->second);
  }

  // throws on error
  inline scribe& by_id(connection_handle hdl) {
    return by_id(hdl, scribes_);
  }

  // throws on error
  inline doorman& by_id(accept_handle hdl) { return by_id(hdl, doormen_); }

  bool invoke_message_from_cache();

  std::map<accept_handle, doorman_pointer> doormen_;
  std::map<connection_handle, scribe_pointer> scribes_;

  middleman& mm_;
  detail::intrusive_partitioned_list<mailbox_element, detail::disposer> cache_;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_ABSTRACT_BROKER_HPP
