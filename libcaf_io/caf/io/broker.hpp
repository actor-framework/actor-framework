/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_IO_BROKER_HPP
#define CAF_IO_BROKER_HPP

#include <map>
#include <vector>

#include "caf/spawn.hpp"
#include "caf/extend.hpp"
#include "caf/local_actor.hpp"

#include "caf/mixin/functor_based.hpp"
#include "caf/mixin/behavior_stack_based.hpp"

#include "caf/policy/not_prioritizing.hpp"
#include "caf/policy/sequential_invoke.hpp"

#include "caf/io/fwd.hpp"
#include "caf/io/accept_handle.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/connection_handle.hpp"
#include "caf/io/network/stream_manager.hpp"
#include "caf/io/network/acceptor_manager.hpp"

namespace caf {
namespace io {

class broker;
class middleman;

using broker_ptr = intrusive_ptr<broker>;

/**
 * A broker mediates between actor systems and other components in the network.
 * @extends local_actor
 */
class broker : public extend<local_actor>::
                      with<mixin::behavior_stack_based<behavior>::impl>,
               public spawn_as_is {
 public:
  using super = combined_type;

  using buffer_type = std::vector<char>;

  /**
   * Manages a low-level IO device for the `broker`.
   */
  class servant {
   public:
    friend class broker;

    virtual ~servant();

   protected:
    virtual void remove_from_broker() = 0;

    virtual message disconnect_message() = 0;

    inline broker* parent() {
      return m_broker.get();
    }

    servant(broker* ptr);

    void set_broker(broker* ptr);

    void disconnect(bool invoke_disconnect_message);

    bool m_disconnected;

    intrusive_ptr<broker> m_broker;
  };

  /**
   * Manages a stream.
   */
  class scribe : public network::stream_manager, public servant {
   public:
    scribe(broker* parent, connection_handle hdl);

    ~scribe();

    /**
     * Implicitly starts the read loop on first call.
     */
    virtual void configure_read(receive_policy::config config) = 0;

    /**
     * Grants access to the output buffer.
     */
    virtual buffer_type& wr_buf() = 0;

    /**
     * Flushes the output buffer, i.e., sends the content of
     *    the buffer via the network.
     */
    virtual void flush() = 0;

    inline connection_handle hdl() const {
      return m_hdl;
    }

    void io_failure(network::operation op) override;

   protected:
    virtual buffer_type& rd_buf() = 0;

    inline new_data_msg& read_msg() {
      return m_read_msg.get_as_mutable<new_data_msg>(0);
    }

    inline const new_data_msg& read_msg() const {
      return m_read_msg.get_as<new_data_msg>(0);
    }

    void remove_from_broker() override;

    message disconnect_message() override;

    void consume(const void* data, size_t num_bytes) override;

    connection_handle m_hdl;

    message m_read_msg;
  };

  using scribe_pointer = intrusive_ptr<scribe>;

  /**
   * Manages incoming connections.
   */
  class doorman : public network::acceptor_manager, public servant {
   public:
    doorman(broker* parent, accept_handle hdl);

    ~doorman();

    inline accept_handle hdl() const {
      return m_hdl;
    }

    void io_failure(network::operation op) override;

    // needs to be launched explicitly
    virtual void launch() = 0;

   protected:
    void remove_from_broker() override;

    message disconnect_message() override;

    inline new_connection_msg& accept_msg() {
      return m_accept_msg.get_as_mutable<new_connection_msg>(0);
    }

    inline const new_connection_msg& accept_msg() const {
      return m_accept_msg.get_as<new_connection_msg>(0);
    }

    accept_handle m_hdl;

    message m_accept_msg;
  };

  using doorman_pointer = intrusive_ptr<doorman>;

  class continuation;

  // a broker needs friends
  friend class scribe;
  friend class doorman;
  friend class continuation;

  ~broker();

  /**
   * Modifies the receive policy for given connection.
   * @param hdl Identifies the affected connection.
   * @param config Contains the new receive policy.
   */
  void configure_read(connection_handle hdl, receive_policy::config config);

  /**
   * Returns the write buffer for given connection.
   */
  buffer_type& wr_buf(connection_handle hdl);

  /**
   * Writes `data` into the buffer for given connection.
   */
  void write(connection_handle hdl, size_t data_size, const void* data);

  /**
   * Sends the content of the buffer for given connection.
   */
  void flush(connection_handle hdl);

  /**
   * Returns the number of open connections.
   */
  inline size_t num_connections() const {
    return m_scribes.size();
  }

  std::vector<connection_handle> connections() const;

  /** @cond PRIVATE */

  template <class F, class... Ts>
  actor fork(F fun, connection_handle hdl, Ts&&... vs) {
    // provoke compile-time errors early
    using fun_res = decltype(fun(this, hdl, std::forward<Ts>(vs)...));
    // prevent warning about unused local type
    static_assert(std::is_same<fun_res, fun_res>::value,
                  "your compiler is lying to you");
    auto i = m_scribes.find(hdl);
    if (i == m_scribes.end()) {
      CAF_LOG_ERROR("invalid handle");
      throw std::invalid_argument("invalid handle");
    }
    auto sptr = i->second;
    CAF_REQUIRE(sptr->hdl() == hdl);
    m_scribes.erase(i);
    return spawn_functor(nullptr, [sptr](broker* forked) {
                                    sptr->set_broker(forked);
                                    forked->m_scribes.insert(
                                      std::make_pair(sptr->hdl(), sptr));
                                  },
                         fun, hdl, std::forward<Ts>(vs)...);
  }

  inline void add_scribe(const scribe_pointer& ptr) {
    m_scribes.insert(std::make_pair(ptr->hdl(), ptr));
  }

  inline void add_doorman(const doorman_pointer& ptr) {
    m_doormen.insert(std::make_pair(ptr->hdl(), ptr));
    if (is_initialized()) {
      ptr->launch();
    }
  }

  void invoke_message(const actor_addr& sender, message_id mid, message& msg);

  void enqueue(const actor_addr&, message_id, message,
               execution_unit*) override;

  template <class F>
  static broker_ptr from(F fun) {
    // transform to STD function here, because GCC is unable
    // to select proper overload otherwise ...
    using fres = decltype(fun((broker*)nullptr));
    std::function<fres(broker*)> stdfun{std::move(fun)};
    return from_impl(std::move(stdfun));
  }

  template <class F, typename T, class... Ts>
  static broker_ptr from(F fun, T&& v, Ts&&... vs) {
    return from(std::bind(fun, std::placeholders::_1, std::forward<T>(v),
                          std::forward<Ts>(vs)...));
  }

  /**
   * Closes all connections and acceptors.
   */
  void close_all();

  /**
   * Closes the connection identified by `handle`.
   * Unwritten data will still be send.
   */
  void close(connection_handle handle);

  /**
   * Closes the acceptor identified by `handle`.
   */
  void close(accept_handle handle);

  class functor_based;

  void launch(bool is_hidden, execution_unit*);

  // <backward_compatibility version="0.9">

  static constexpr auto at_least = receive_policy_flag::at_least;

  static constexpr auto at_most = receive_policy_flag::at_most;

  static constexpr auto exactly = receive_policy_flag::exactly;

  void receive_policy(connection_handle hdl, receive_policy_flag flag,
                      size_t num_bytes) CAF_DEPRECATED {
    configure_read(hdl, receive_policy::config{flag, num_bytes});
  }

  // </backward_compatibility>

 protected:
  broker();

  void cleanup(uint32_t reason);

  virtual behavior make_behavior() = 0;

  /** @endcond */

  inline middleman& parent() {
    return m_mm;
  }

  network::multiplexer& backend();

 private:
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
    return by_id(hdl, m_scribes);
  }

  // throws on error
  inline doorman& by_id(accept_handle hdl) { return by_id(hdl, m_doormen); }

  bool invoke_message_from_cache();

  void erase_io(int id);

  void erase_acceptor(int id);

  std::map<accept_handle, doorman_pointer> m_doormen;
  std::map<connection_handle, scribe_pointer> m_scribes;

  policy::not_prioritizing m_priority_policy;
  policy::sequential_invoke m_invoke_policy;

  middleman& m_mm;
};

class broker::functor_based : public extend<broker>::
                                     with<mixin::functor_based> {
 public:
  using super = combined_type;

  template <class... Ts>
  functor_based(Ts&&... vs) : super(std::forward<Ts>(vs)...) {
    // nop
  }

  ~functor_based();

  behavior make_behavior() override;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_BROKER_HPP
