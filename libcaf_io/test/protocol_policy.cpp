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

#define CAF_SUITE protocol_policy

//#include "caf/io/protocol_policy.hpp"

#include "caf/test/dsl.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/config.hpp"

#include "caf/io/network/native_socket.hpp"
// TODO: {receive_buffer => byte_buffer} (and use `byte` instead of `char`)
#include "caf/io/network/receive_buffer.hpp"

using namespace caf;
using namespace caf::io;

using network::native_socket;

namespace {

// -- forward declarations -----------------------------------------------------

struct dummy_device;
struct newb_base;
struct protocol_policy_base;

template <class T>
struct protocol_policy;

// -- aliases ------------------------------------------------------------------

using byte_buffer = network::receive_buffer;

// -- dummy headers ------------------------------------------------------------

struct basp_header {
  actor_id from;
  actor_id to;
};

struct ordering_header {
  int32_t seq_nr;
};

// -- message types ------------------------------------------------------------

struct new_basp_message {
  basp_header header;
  const char* payload;
  size_t payload_size;
};

// -- new broker classes -------------------------------------------------------

struct transport_policy {
  virtual ~transport_policy() {
    // nop
  }

  virtual error write_some(network::native_socket) {
    return none;
  }

  byte_buffer* wr_buf() {
    return &send_buffer;
  }


  template <class T>
  expected<T> read_some(protocol_policy<T>& policy) {
    auto err = read_some();
    if (err)
      return err;
    return policy.read(receive_buffer.data(), receive_buffer.size());
  }

  virtual error read_some() {
    return none;
  }

  byte_buffer receive_buffer;
  byte_buffer send_buffer;
};

using transport_policy_ptr = std::unique_ptr<transport_policy>;

struct accept_policy {
  virtual std::pair<native_socket, transport_policy_ptr> accept() = 0;
  virtual void init(newb_base&) = 0;
};

struct protocol_policy_base {
  virtual ~protocol_policy_base() {
    // nop
  }

  virtual void write_header(byte_buffer& buf, size_t offset) = 0;

  virtual size_t offset() const noexcept = 0;
};

template <class T>
struct protocol_policy : protocol_policy_base {
  virtual ~protocol_policy() {
    // nop
  }

  virtual expected<T> read(char* bytes, size_t count) = 0;

  void write_header(byte_buffer&, size_t) override {
    // nop
  }
};

template <class T>
using protocol_policy_ptr = std::unique_ptr<protocol_policy<T>>;

struct basp_policy {
  static constexpr size_t offset = sizeof(basp_header);
  using type = new_basp_message;
  using result_type = expected<new_basp_message>;
  result_type read(char*, size_t) {
    new_basp_message msg;
    return msg;
  }

};

template <class Next>
struct ordering {
  static constexpr size_t offset = Next::offset + sizeof(ordering_header);
  using type = typename Next::type;
  using result_type = typename Next::result_type;
  Next next;
  result_type read(char* bytes, size_t count) {
    return next.read(bytes, count);
  }

};

template <class T>
struct policy_impl : protocol_policy<typename T::type> {
  T impl;
  policy_impl() : impl() {
    // nop
  }

  typename T::result_type read(char* bytes, size_t count) override {
    return impl.read(bytes, count);
  }

  size_t offset() const noexcept override {
    return T::offset;
  }
};

struct write_handle {
  protocol_policy_base* protocol;
  byte_buffer* buf;
  size_t header_offset;

  ~write_handle() {
    protocol->write_header(*buf, header_offset);
    // TODO: maybe trigger device
  }
};

template <class Message>
struct newb {
  std::unique_ptr<transport_policy> device;
  std::unique_ptr<protocol_policy<Message>> policy;

  write_handle wr_buf() {
    auto buf = device->wr_buf();
    auto header_size = policy->offset();
    auto header_offset = buf->size();
    buf->resize(buf->size() + header_size);
    return {policy.get(), buf, header_offset};
  }

  void flush() {

  }

  error read_event() {
    auto maybe_msg = device->read_some(*policy);
    if (!maybe_msg)
      return std::move(maybe_msg.error());
    // TODO: create message on the stack and call message handler
    handle(*maybe_msg);
  }

  void write_event() {
    // device->write_some();
  }

  virtual void handle(Message& msg) = 0;
};

struct basp_newb : newb<new_basp_message> {
  void handle(new_basp_message&) override {
    // nop
  }
};

/*
behavior my_broker(newb<new_data_msg>* self) {
  // nop ...
}

template <class AcceptPolicy, class ProtocolPolicy>
struct newb_acceptor {
  std::unique_ptr<AcceptPolicy> acceptor;

  // read = accept
  error read_event() {
    auto [sock, trans_pol] = acceptor.accept();
    auto worker = sys.middleman.spawn_client<ProtocolPolicy>(
      sock, std::move(trans_pol), fork_behavior);
    acceptor.init(worker);
  }
};
*/

// client: sys.middleman().spawn_client<policy>(sock, std::move(transport_policy_impl), my_client);
// server: sys.middleman().spawn_server<policy>(sock, std::move(accept_policy_impl), my_server);

struct fixture {
  basp_newb self;

  fixture() {
    self.device.reset(new transport_policy);
    self.policy.reset(new policy_impl<ordering<basp_policy>>);
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(protocol_policy_tests, fixture)

CAF_TEST(ordering and basp) {
}

CAF_TEST_FIXTURE_SCOPE_END()
