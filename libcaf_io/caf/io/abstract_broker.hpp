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

#include <vector>
#include <unordered_map>

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

    void consume(const void* data, size_t num_bytes) override;

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

    connection_handle hdl_;

    message read_msg_;
  };

  using scribe_ptr = intrusive_ptr<scribe>;

  /// Manages incoming connections.
  class doorman : public network::acceptor_manager, public servant {
  public:
    doorman(abstract_broker* parent, accept_handle hdl, uint16_t local_port);

    ~doorman();

    inline accept_handle hdl() const {
      return hdl_;
    }

    void io_failure(network::operation op) override;

    // needs to be launched explicitly
    virtual void launch() = 0;

    uint16_t port() const {
      return port_;
    }

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
    uint16_t port_;
  };

  using doorman_ptr = intrusive_ptr<doorman>;

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

  inline middleman& parent() {
    return mm_;
  }

  inline void add_scribe(const scribe_ptr& ptr) {
    scribes_.emplace(ptr->hdl(), ptr);
  }

  connection_handle add_tcp_scribe(const std::string& host, uint16_t port);

  void assign_tcp_scribe(connection_handle hdl);

  connection_handle add_tcp_scribe(network::native_socket fd);

  inline void add_doorman(const doorman_ptr& ptr) {
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

  /// Returns the local port associated to `hdl` or `0` if `hdl` is invalid.
  uint16_t local_port(accept_handle hdl);

  /// Returns the handle associated to given local `port` or `none`.
  optional<accept_handle> hdl_by_port(uint16_t port);

  void invoke_message(mailbox_element_ptr& msg);

  void invoke_message(const actor_addr& sender,
                      message_id mid, message& msg);

  /// Closes all connections and acceptors.
  void close_all();

  /// Closes the connection or acceptor identified by `handle`.
  /// Unwritten data will still be send.
  template <class Handle>
  void close(Handle hdl) {
    by_id(hdl).stop_reading();
  }

  /// Checks whether `hdl` is assigned to broker.
  template <class Handle>
  bool valid(Handle hdl) {
    return get_map(hdl).count(hdl) > 0;
  }

protected:
  abstract_broker();

  abstract_broker(middleman& parent_ref);

  using doorman_map = std::unordered_map<accept_handle, doorman_ptr>;

  using scribe_map = std::unordered_map<connection_handle, scribe_ptr>;

  // meta programming utility
  inline doorman_map& get_map(accept_handle) {
    return doormen_;
  }

  // meta programming utility
  inline scribe_map& get_map(connection_handle) {
    return scribes_;
  }

  // meta programming utility (not implemented)
  static doorman_ptr ptr_of(accept_handle);

  // meta programming utility (not implemented)
  static scribe_ptr ptr_of(connection_handle);

  /// @endcond

  network::multiplexer& backend();

  /// Returns a `scribe` or `doorman` identified by `hdl`.
  template <class Handle>
  auto by_id(Handle hdl) -> decltype(*ptr_of(hdl)) {
    auto& elements = get_map(hdl);
    auto i = elements.find(hdl);
    if (i == elements.end())
      throw std::invalid_argument("invalid handle");
    return *(i->second);
  }

  /// Returns an intrusive pointer to a `scribe` or `doorman`
  /// identified by `hdl` and remove it from this broker.
  template <class Handle>
  auto take(Handle hdl) -> decltype(ptr_of(hdl)) {
    using std::swap;
    auto& elements = get_map(hdl);
    decltype(ptr_of(hdl)) result;
    auto i = elements.find(hdl);
    if (i == elements.end())
      throw std::invalid_argument("invalid handle");
    swap(result, i->second);
    elements.erase(i);
    return result;
  }

  bool invoke_message_from_cache();

private:
  doorman_map doormen_;
  scribe_map scribes_;
  middleman& mm_;
  detail::intrusive_partitioned_list<mailbox_element, detail::disposer> cache_;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_ABSTRACT_BROKER_HPP
