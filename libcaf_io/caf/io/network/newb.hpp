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

#include "caf/config.hpp"

#include <cstdint>
#include <cstring>
#include <tuple>

#include "caf/test/dsl.hpp"

#include "caf/callback.hpp"
#include "caf/config.hpp"
#include "caf/detail/call_cfun.hpp"
#include "caf/detail/enum_to_string.hpp"
#include "caf/detail/socket_guard.hpp"
#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/mixin/behavior_changer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf {
namespace io {
namespace network {

// -- forward declarations -----------------------------------------------------

template <class T>
struct protocol_policy;

template <class T>
struct newb;


} // namespace io
} // namespace network

// -- required to make newb an actor ------------------------------------------

template <class T>
class behavior_type_of<io::network::newb<T>> {
public:
  using type = behavior;
};

namespace io {
namespace network {

// -- aliases ------------------------------------------------------------------

using byte_buffer = std::vector<char>;
using header_writer = caf::callback<byte_buffer&>;

// -- transport policy ---------------------------------------------------------

struct transport_policy {
  transport_policy() : received_bytes{0}, max_consecutive_reads{50} {
    // nop
  }

  virtual ~transport_policy() {
    // nop
  }

  virtual error write_some(network::event_handler*) {
    return none;
  }

  virtual error read_some(network::event_handler*) {
    return none;
  }

  virtual bool should_deliver() {
    return true;
  }

  virtual void prepare_next_read(network::event_handler*) {
    // nop
  }

  virtual void prepare_next_write(network::event_handler*) {
    // nop
  }

  virtual void configure_read(receive_policy::config) {
    // nop
  }

  virtual void flush(network::event_handler*) {
    // nop
  }

  inline byte_buffer& wr_buf() {
    return offline_buffer;
  }

  template <class T>
  error read_some(network::event_handler* parent, protocol_policy<T>& policy) {
    CAF_LOG_TRACE("");
    auto mcr = max_consecutive_reads;
    for (size_t i = 0; i < mcr; ++i) {
      auto res = read_some(parent);
      // The return statements seems weird, needs cleanup.
      if (res)
        return res;
      if (should_deliver()) {
        res = policy.read(receive_buffer.data(), received_bytes);
        prepare_next_read(parent);
        if (!res)
          return res;
      }
    }
    return none;
  }

  size_t received_bytes;
  size_t max_consecutive_reads;

  byte_buffer offline_buffer;
  byte_buffer receive_buffer;
  byte_buffer send_buffer;
};

using transport_policy_ptr = std::unique_ptr<transport_policy>;

// -- accept policy ------------------------------------------------------------

template <class Message>
struct accept_policy {
  virtual ~accept_policy() {
    // nop
  }

  virtual std::pair<native_socket, transport_policy_ptr>
  accept(network::event_handler*) = 0;

  virtual void init(newb<Message>&) = 0;
};

// -- protocol policy ----------------------------------------------------------

struct protocol_policy_base {
  virtual ~protocol_policy_base() {
    // nop
  }

  virtual error read(char* bytes, size_t count) = 0;

  virtual error timeout(atom_value, uint32_t) = 0;

  virtual void write_header(byte_buffer&, header_writer*) = 0;

  virtual void prepare_for_sending(byte_buffer&, size_t, size_t) = 0;
};

template <class T>
struct protocol_policy : protocol_policy_base {
  using message_type = T;
  virtual ~protocol_policy() override {
    // nop
  }
};

template <class T>
using protocol_policy_ptr = std::unique_ptr<protocol_policy<T>>;

// -- new broker classes -------------------------------------------------------

/// @relates newb
/// Returned by funtion wr_buf of newb.
template <class Message>
struct write_handle {
  newb<Message>* parent;
  protocol_policy_base* protocol;
  byte_buffer* buf;
  size_t header_start;
  size_t header_len;

  ~write_handle() {
    // Can we calculate added bytes for datagram things?
    auto payload_size = buf->size() - (header_start + header_len);
    protocol->prepare_for_sending(*buf, header_start, payload_size);
    parent->flush();
  }
};

template <class Message>
struct newb : public extend<scheduled_actor, newb<Message>>::template
                     with<mixin::sender, mixin::requester,
                          mixin::behavior_changer>,
              public dynamically_typed_actor_base,
              public network::event_handler {
  using super = typename extend<scheduled_actor, newb<Message>>::
    template with<mixin::sender, mixin::requester, mixin::behavior_changer>;

  using signatures = none_t;

  // -- constructors and destructors -------------------------------------------

  newb(actor_config& cfg, default_multiplexer& dm, native_socket sockfd)
      : super(cfg),
        event_handler(dm, sockfd) {
    // nop
  }

  newb() = default;
  newb(newb<Message>&&) = default;

  ~newb() override {
    // nop
  }

  // -- overridden modifiers of abstract_actor ---------------------------------

  void enqueue(mailbox_element_ptr ptr, execution_unit*) override {
    CAF_PUSH_AID(this->id());
    scheduled_actor::enqueue(std::move(ptr), &backend());
  }

  void enqueue(strong_actor_ptr src, message_id mid, message msg,
               execution_unit*) override {
    enqueue(make_mailbox_element(std::move(src), mid, {}, std::move(msg)),
            &backend());
  }

  resumable::subtype_t subtype() const override {
    return resumable::io_actor;
  }

  // -- overridden modifiers of local_actor ------------------------------------

  void launch(execution_unit* eu, bool lazy, bool hide) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_ASSERT(eu != nullptr);
    CAF_ASSERT(eu == &backend());
    CAF_LOG_TRACE(CAF_ARG(lazy) << CAF_ARG(hide));
    // add implicit reference count held by middleman/multiplexer
    if (!hide)
      super::register_at_system();
    if (lazy && super::mailbox().try_block())
      return;
    intrusive_ptr_add_ref(super::ctrl());
    eu->exec_later(this);
  }

  void initialize() override {
    CAF_LOG_TRACE("");
    init_newb();
    auto bhvr = make_behavior();
    CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                             << CAF_ARG(this->has_behavior()));
    if (bhvr) {
      // make_behavior() did return a behavior instead of using become()
      CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
      this->become(std::move(bhvr));
    }
  }

  bool cleanup(error&& reason, execution_unit* host) override {
    CAF_LOG_TRACE(CAF_ARG(reason));
    // TODO: Ask policies, close socket.
    return local_actor::cleanup(std::move(reason), host);
  }

  // -- overridden modifiers of resumable --------------------------------------

  network::multiplexer::runnable::resume_result resume(execution_unit* ctx,
                                                       size_t mt) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_ASSERT(ctx != nullptr);
    CAF_ASSERT(ctx == &backend());
    return scheduled_actor::resume(ctx, mt);
  }

  // -- overridden modifiers of event handler ----------------------------------

  void handle_event(network::operation op) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE("");
    switch (op) {
      case io::network::operation::read:
        read_event();
        break;
      case io::network::operation::write:
        write_event();
        break;
      case io::network::operation::propagate_error:
        handle_error();
    }
  }

  void removed_from_loop(network::operation op) override {
    CAF_LOG_DEBUG("newb removed from loop: " << to_string(op));
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE(CAF_ARG(op));
    switch (op) {
      case network::operation::read:  break;
      case network::operation::write: break;
      case network::operation::propagate_error: ; // nop
    }
    // nop
  }

  // -- members ----------------------------------------------------------------

  void init_newb() {
    CAF_LOG_TRACE("");
    super::setf(super::is_initialized_flag);
  }

  void start() {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE("");
    CAF_LOG_DEBUG("starting newb");
    activate();
    if (transport)
      transport->prepare_next_read(this);
  }

  void stop() {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE("");
    close_read_channel();
    passivate();
  }

  write_handle<Message> wr_buf(header_writer* hw) {
    // TODO: We somehow need to tell the transport policy how much we've
    // written to enable it to split the buffer into datagrams.
    auto& buf = transport->wr_buf();
    auto hstart = buf.size();
    protocol->write_header(buf, hw);
    auto hlen = buf.size() - hstart;
    return {this, protocol.get(), &buf, hstart, hlen};
  }

  void flush() {
    transport->flush(this);
  }

  error read_event() {
    return transport->read_some(this, *protocol);
  }

  void write_event() {
    transport->write_some(this);
  }

  void handle_error() {
    CAF_CRITICAL("got error to handle: not implemented");
  }

  // Protocol policies can set timeouts using a custom message.
  template<class Rep = int, class Period = std::ratio<1>>
  void set_timeout(std::chrono::duration<Rep, Period> timeout,
                   atom_value atm, uint32_t id) {
    this->delayed_send(this, timeout, atm, id);
    // TODO: Use actor clock.
    // TODO: Make this system messages and handle them separately.
  }

  // Allow protocol policies to enqueue a data for sending. Probably required for
  // reliability to send ACKs. The problem is that only the headers of policies
  // lower, well closer to the transport, should write their headers. So we're
  // facing a similiar porblem to slicing here.
  // void (char* bytes, size_t count);

  /// Returns the `multiplexer` running this broker.
  network::multiplexer& backend() {
    return event_handler::backend();
  }

  virtual void handle(Message& msg) = 0;

  // Currently has to handle timeouts as well, see handler below.
  virtual behavior make_behavior() = 0; /*{
    return {
      [=](atom_value atm, uint32_t id) {
        protocol->timeout(atm, id);
      }
    };
  }*/

  void configure_read(receive_policy::config config) {
    transport->configure_read(config);
  }

  /// @cond PRIVATE

  template <class... Ts>
  void eq_impl(message_id mid, strong_actor_ptr sender,
               execution_unit* ctx, Ts&&... xs) {
    enqueue(make_mailbox_element(std::move(sender), mid,
                                 {}, std::forward<Ts>(xs)...),
            ctx);
  }

  /// @endcond

  // -- policies ---------------------------------------------------------------

  std::unique_ptr<transport_policy> transport;
  std::unique_ptr<protocol_policy<Message>> protocol;
};

// -- new broker acceptor ------------------------------------------------------

template <class Message>
struct newb_acceptor : public network::event_handler {

  // -- constructors and destructors -------------------------------------------

  newb_acceptor(default_multiplexer& dm, native_socket sockfd)
      : event_handler(dm, sockfd) {
    // nop
  }

  // -- overridden modifiers of event handler ----------------------------------

  void handle_event(network::operation op) override {
    CAF_LOG_DEBUG("new event: " << to_string(op));
    switch (op) {
      case network::operation::read:
        read_event();
        break;
      case network::operation::write:
        // nop
        break;
      case network::operation::propagate_error:
        CAF_LOG_DEBUG("acceptor got error operation");
        break;
    }
  }

  void removed_from_loop(network::operation op) override {
    CAF_LOG_TRACE(CAF_ARG(op));
    switch (op) {
      case network::operation::read:  break;
      case network::operation::write: break;
      case network::operation::propagate_error: ; // nop
    }
  }

  // -- members ----------------------------------------------------------------

  error read_event() {
    native_socket sock;
    transport_policy_ptr transport;
    std::tie(sock, transport) = acceptor->accept(this);
    auto en = create_newb(sock, std::move(transport));
    if (!en)
      return std::move(en.error());
    auto ptr = caf::actor_cast<caf::abstract_actor*>(*en);
    CAF_ASSERT(ptr != nullptr);
    auto& ref = dynamic_cast<newb<Message>&>(*ptr);
    acceptor->init(ref);
    return none;
  }

  void start() {
    activate();
  }

  void stop() {
    CAF_LOG_TRACE(CAF_ARG2("fd", fd()));
    close_read_channel();
    passivate();
  }

  virtual expected<actor> create_newb(native_socket sock,
                                      transport_policy_ptr pol) = 0;

  // TODO: Has to implement a static create socket function ...

  std::unique_ptr<accept_policy<Message>> acceptor;
};

// -- factories ----------------------------------------------------------------

template <class Newb>
actor make_newb(actor_system& sys, native_socket sockfd) {
  auto& mpx = dynamic_cast<default_multiplexer&>(sys.middleman().backend());
  actor_config acfg{&mpx};
  auto res = sys.spawn_impl<Newb, hidden + lazy_init>(acfg, mpx, sockfd);
  return actor_cast<actor>(res);
}

// TODO: I feel like this should include the ProtocolPolicy somehow.
template <class NewbAcceptor, class AcceptPolicy>
std::unique_ptr<NewbAcceptor> make_newb_acceptor(actor_system& sys,
                                                  uint16_t port,
                                                  const char* addr = nullptr,
                                                  bool reuse_addr = false) {
  auto sockfd = NewbAcceptor::create_socket(port, addr, reuse_addr);
  // new_tcp_acceptor_impl(port, addr, reuse_addr);
  if (!sockfd) {
    CAF_LOG_DEBUG("Could not open " << CAF_ARG(port) << CAF_ARG(addr));
    return nullptr;
  }
  auto& mpx = dynamic_cast<default_multiplexer&>(sys.middleman().backend());
  std::unique_ptr<NewbAcceptor> ptr{new NewbAcceptor(mpx, *sockfd)};
  ptr->acceptor.reset(new AcceptPolicy);
  ptr->start();
  return ptr;
}


} // namespace network
} // namespace io
} // namespace caf

