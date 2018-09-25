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

#include "caf/actor_clock.hpp"
#include "caf/config.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/call_cfun.hpp"
#include "caf/detail/enum_to_string.hpp"
#include "caf/detail/socket_guard.hpp"
#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/rw_state.hpp"
#include "caf/mixin/behavior_changer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/policy/accept.hpp"
#include "caf/policy/protocol.hpp"
#include "caf/policy/transport.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf {
namespace io {

// -- forward declarations -----------------------------------------------------

template <class T>
struct protocol_policy;

template <class T>
struct newb;

} // namespace io

// -- required to make newb an actor ------------------------------------------

template <class T>
class behavior_type_of<io::newb<T>> {
public:
  using type = behavior;
};

namespace io {

// -- aliases ------------------------------------------------------------------

using byte_buffer = caf::policy::byte_buffer;
using header_writer = caf::policy::header_writer;

// -- newb base ----------------------------------------------------------------

struct newb_base : public network::event_handler {
  newb_base(network::default_multiplexer& dm, network::native_socket sockfd);

  virtual void start() = 0;

  virtual void stop() = 0;

  virtual void io_error(network::operation op, error err) = 0;

  virtual void start_reading() = 0;

  virtual void stop_reading() = 0;

  virtual void start_writing() = 0;

  virtual void stop_writing() = 0;

};

// -- new broker classes -------------------------------------------------------

/// @relates newb
/// Returned by funtion wr_buf of newb.
template <class Message>
struct write_handle {
  newb<Message>* parent;
  policy::protocol_base* protocol;
  byte_buffer* buf;
  size_t header_start;
  size_t header_len;

  ~write_handle() {
    // Can we calculate added bytes for datagram things?
    auto payload_size = buf->size() - (header_start + header_len);
    protocol->prepare_for_sending(*buf, header_start, 0, payload_size);
    parent->flush();
  }
};

template <class Message>
struct newb : public extend<scheduled_actor, newb<Message>>::template
                     with<mixin::sender, mixin::requester,
                          mixin::behavior_changer>,
              public dynamically_typed_actor_base,
              public newb_base {
  using super = typename extend<scheduled_actor, newb<Message>>::
    template with<mixin::sender, mixin::requester, mixin::behavior_changer>;

  using signatures = none_t;

  using message_type = Message;

  // -- constructors and destructors -------------------------------------------

  newb(actor_config& cfg, network::default_multiplexer& dm,
       network::native_socket sockfd,
       policy::transport_ptr transport,
       policy::protocol_ptr<Message> protocol)
      : super(cfg),
        newb_base(dm, sockfd),
        trans(std::move(transport)),
        proto(std::move(protocol)),
        value_(strong_actor_ptr{}, make_message_id(),
               mailbox_element::forwarding_stack{}, Message{}),
        reading_(false),
        writing_(false) {
    CAF_LOG_TRACE("");
    scheduled_actor::set_timeout_handler([&](timeout_msg& msg) {
      proto->timeout(msg.type, msg.timeout_id);
    });
    proto->init(this);
  }

  newb() = default;

  newb(newb<Message>&&) = default;

  ~newb() override {
    CAF_LOG_TRACE("");
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

  const char* name() const override {
    return "newb";
  }

  // -- overridden modifiers of local_actor ------------------------------------

  void launch(execution_unit* eu, bool lazy, bool hide) override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_ASSERT(eu != nullptr);
    CAF_ASSERT(eu == &backend());
    CAF_LOG_TRACE(CAF_ARG(lazy) << CAF_ARG(hide));
    // Add implicit reference count held by middleman/multiplexer.
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
      // make_behavior() did return a behavior instead of using become().
      CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
      this->become(std::move(bhvr));
    }
  }

  bool cleanup(error&& reason, execution_unit* host) override {
    CAF_LOG_TRACE(CAF_ARG(reason));
    stop();
    // TODO: Should policies be notified here?
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
      case network::operation::read:
        reading_ = false;
        break;
      case network::operation::write:
        writing_ = false;
        break;
      case network::operation::propagate_error: ; // nop
    }
    // Event handler reference no longer necessary.
    if (!reading_ && !writing_)
      intrusive_ptr_release(super::ctrl());
  }

  // -- base requirements ------------------------------------------------------

  void start() override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE("");
    // This is our own reference used to manage the lifetime matching
    // as an event handler.
    if (!reading_ && !writing_)
      intrusive_ptr_add_ref(super::ctrl());
    start_reading();
    if (trans)
      trans->prepare_next_read(this);
  }

  void stop() override {
    CAF_PUSH_AID_FROM_PTR(this);
    CAF_LOG_TRACE("");
    close_read_channel();
    stop_reading();
    stop_writing();
  }

  void io_error(network::operation op, error err) override {
    using network::operation;
    if (!this->getf(this->is_cleaned_up_flag)) {
      auto mptr = make_mailbox_element(nullptr, invalid_message_id, {},
                                       io_error_msg{op, std::move(err)});
      switch (scheduled_actor::consume(*mptr)) {
        case im_success:
          this->finalize();
          break;
        case im_skipped:
          this->push_to_cache(std::move(mptr));
          break;
        case im_dropped:
          CAF_LOG_INFO("broker dropped read error message");
          break;
      }
    }
    switch (op) {
      case operation::read:
        stop_reading();
        break;
      case operation::write:
        stop_writing();
        break;
      case operation::propagate_error:
        // TODO: What should happen here?
        break;
    }
  }

  void start_reading() override {
    if (!reading_) {
      event_handler::activate();
      reading_ = true;
    }
  }

  void stop_reading() override {
    passivate();
  }

  void start_writing() override {
    if (!writing_) {
      event_handler::backend().add(network::operation::write, fd(), this);
      writing_ = true;
    }
  }

  void stop_writing() override {
    event_handler::backend().del(network::operation::write, fd(), this);
  }

  // -- members ----------------------------------------------------------------

  void init_newb() {
    CAF_LOG_TRACE("");
    super::setf(super::is_initialized_flag);
  }

  /// Get a write buffer to write data to be sent by this broker.
  write_handle<Message> wr_buf(header_writer* hw) {
    auto& buf = trans->wr_buf();
    auto hstart = buf.size();
    proto->write_header(buf, hw);
    auto hlen = buf.size() - hstart;
    return {this, proto.get(), &buf, hstart, hlen};
  }

  byte_buffer& wr_buf() {
    return trans->wr_buf();
  }

  void flush() {
    trans->flush(this);
  }

  void read_event() {
    auto err = trans->read_some(this, *proto);
    if (err)
      io_error(network::operation::read, std::move(err));
  }

  void write_event() {
    if (trans->write_some(this) == network::rw_state::failure)
      io_error(network::operation::write, sec::runtime_error);
  }

  void handle_error() {
    CAF_CRITICAL("got error to handle: not implemented");
  }

  /// Set a timeout for a protocol policy layer.
  template<class Rep = int, class Period = std::ratio<1>>
  void set_timeout(std::chrono::duration<Rep, Period> timeout,
                   atom_value atm, uint32_t id) {
    //auto n = actor_clock::clock_type::now();
    //scheduled_actor::clock().set_ordinary_timeout(n + timeout, this, atm, id);
    this->delayed_send(this, timeout, atm, id);
  }

  /// Returns the `multiplexer` running this broker.
  network::multiplexer& backend() {
    return event_handler::backend();
  }

  /// Pass a message from a protocol policy layer to the broker for processing.
  void handle(Message& m) {
    std::swap(msg(), m);
    scheduled_actor::activate(scheduled_actor::context(), value_);
  }

  /// Override this to set the behavior of the broker.
  virtual behavior make_behavior() {
    behavior res;
    if (this->initial_behavior_fac_) {
      res = this->initial_behavior_fac_(this);
      this->initial_behavior_fac_ = nullptr;
    }
    return res;
  }

  /// Configure the number of bytes read for the next packet. (Can be ignored by
  /// the transport policy if its protocol does not support this functionality.)
  void configure_read(receive_policy::config config) {
    trans->configure_read(config);
  }

  /// @cond PRIVATE

  template <class... Ts>
  void eq_impl(message_id mid, strong_actor_ptr sender,
               execution_unit* ctx, Ts&&... xs) {
    enqueue(make_mailbox_element(std::move(sender), mid,
                                 {}, std::forward<Ts>(xs)...),
            ctx);
  }

  // -- policies ---------------------------------------------------------------

  policy::transport_ptr trans;
  policy::protocol_ptr<Message> proto;

  /// @endcond

private:
  // -- message element --------------------------------------------------------

  Message& msg() {
    return value_.template get_mutable_as<Message>(0);
  }

  mailbox_element_vals<Message> value_;
  bool reading_;
  bool writing_;
};


/// Convenience template alias for declaring state-based brokers.
template <class Message, class State>
using stateful_newb = stateful_actor<State, newb<Message>>;

// Primary template.
template<class T>
struct function_traits : function_traits<decltype(&T::operator())> {
};

// Partial specialization for function type.
template<class R, class... Args>
struct function_traits<R(Args...)> {
    using result_type = R;
    using argument_types = std::tuple<Args...>;
};

// Partial specialization for function pointer.
template<class R, class... Args>
struct function_traits<R (*)(Args...)> {
    using result_type = R;
    using argument_types = std::tuple<Args...>;
};

// Partial specialization for std::function.
template<class R, class... Args>
struct function_traits<std::function<R(Args...)>> {
    using result_type = R;
    using argument_types = std::tuple<Args...>;
};

// Partial specialization for pointer-to-member-function (i.e., operator()'s).
template<class T, class R, class... Args>
struct function_traits<R (T::*)(Args...)> {
    using result_type = R;
    using argument_types = std::tuple<Args...>;
};

template<class T, class R, class... Args>
struct function_traits<R (T::*)(Args...) const> {
    using result_type = R;
    using argument_types = std::tuple<Args...>;
};

template<class T>
using first_argument_type
 = typename std::tuple_element<0, typename function_traits<T>::argument_types>::type;

/// Spawns a new "newb" broker.
template <class Protocol, spawn_options Os = no_spawn_options, class F, class... Ts>
typename infer_handle_from_fun<F>::type
spawn_newb(actor_system& sys, F fun, policy::transport_ptr transport,
           network::native_socket sockfd, Ts&&... xs) {
  using impl = typename infer_handle_from_fun<F>::impl;
  using first = first_argument_type<F>;
  using message = typename std::remove_pointer<first>::type::message_type;
  auto& dm = dynamic_cast<network::default_multiplexer&>(sys.middleman().backend());
  // Setup the config.
  actor_config cfg(&dm);
  detail::init_fun_factory<impl, F> fac;
  auto init_fun = fac(std::move(fun), std::forward<Ts>(xs)...);
  cfg.init_fun = [init_fun](local_actor* self) mutable -> behavior {
    return init_fun(self);
  };
  policy::protocol_ptr<message> proto(new Protocol());
  auto res = sys.spawn_class<impl, Os>(cfg, dm, sockfd, std::move(transport),
                                       std::move(proto));
  // Get a reference to the newb type.
  auto ptr = caf::actor_cast<caf::abstract_actor*>(res);
  CAF_ASSERT(ptr != nullptr);
  auto& ref = dynamic_cast<newb<message>&>(*ptr);
  // Start the event handler.
  ref.start();
  return res;
}

/// Spawn a new "newb" broker client to connect to `host`:`port`.
template <class Protocol, spawn_options Os = no_spawn_options, class F, class... Ts>
expected<typename infer_handle_from_fun<F>::type>
spawn_client(actor_system& sys, F fun, policy::transport_ptr transport,
             std::string host, uint16_t port, Ts&&... xs) {
  expected<network::native_socket> esock = transport->connect(host, port);
  if (!esock)
    return std::move(esock.error());
  return spawn_newb<Protocol>(sys, fun, std::move(transport), *esock,
                              std::forward<Ts>(xs)...);
}

// -- new broker acceptor ------------------------------------------------------

template <class Protocol, class Fun, class... Ts>
struct newb_acceptor : public newb_base, public caf::ref_counted {
  using newb_type = typename std::remove_pointer<first_argument_type<Fun>>::type;
  using message_type = typename newb_type::message_type;

  // -- constructors and destructors -------------------------------------------

  newb_acceptor(network::default_multiplexer& dm, network::native_socket sockfd,
                Fun f, policy::accept_ptr<message_type> pol, Ts&&... xs)
      : newb_base(dm, sockfd),
        accept_pol(std::move(pol)),
        fun_(std::move(f)),
        reading_(false),
        writing_(false),
        args_(std::forward<Ts>(xs)...) {
    // nop
    if (sockfd == io::network::invalid_native_socket)
      CAF_LOG_ERROR("Creating newb with invalid socket");
  }

  newb_acceptor(const newb_acceptor& other) = delete;

  ~newb_acceptor() {
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
        // Required to multiplex over a single socket.
        write_event();
        break;
      case network::operation::propagate_error:
        CAF_LOG_DEBUG("acceptor got error operation");
        break;
    }
  }

  void removed_from_loop(network::operation op) override {
    CAF_LOG_TRACE(CAF_ARG(op));
    CAF_LOG_DEBUG("newb removed from loop: " << to_string(op));
    switch (op) {
      case network::operation::read:
        reading_ = false;
        break;
      case network::operation::write:
        writing_ = false;
        break;
      case network::operation::propagate_error: ; // nop
    }
    // Quit if there is nothing left to do.
    if (!reading_ && !writing_)
      deref();
  }

  // -- base requirements ------------------------------------------------------

  void start() override {
    ref();
    start_reading();
    backend().post([]() {
      // nop
    });
  }

  void stop() override {
    CAF_LOG_TRACE(CAF_ARG2("fd", fd()));
    close_read_channel();
    stop_reading();
    stop_writing();
  }

  void io_error(network::operation op, error err) override {
    CAF_LOG_ERROR("operation " << to_string(op) << " failed: "
                  << backend().system().render(err));
    stop();
  }

  void start_reading() override {
    if (!reading_) {
      activate();
      reading_ = true;
    }
  }

  void stop_reading() override {
    passivate();
  }

  void start_writing() override {
    if (!writing_) {
      event_handler::backend().add(network::operation::write, fd(), this);
      writing_ = true;
    }
  }

  void stop_writing() override {
    event_handler::backend().del(network::operation::write, fd(), this);
  }

  // -- members ----------------------------------------------------------------

  void read_event() {
    if (accept_pol->manual_read) {
      accept_pol->read_event(this);
    } else {
      using newb_ptr = first_argument_type<Fun>;
      using newb_type = typename std::remove_pointer<newb_ptr>::type;
      using message = typename newb_type::message_type;
      network::native_socket sock;
      policy::transport_ptr transport;
      std::tie(sock, transport) = accept_pol->accept_event(this);
      if (sock == network::invalid_native_socket) {
        CAF_LOG_ERROR("failed to create socket for new endpoint");
        return;
      }
      auto en = create_newb(sock, std::move(transport));
      if (!en) {
        io_error(network::operation::read, std::move(en.error()));
        return;
      }
      auto ptr = caf::actor_cast<caf::abstract_actor*>(*en);
      CAF_ASSERT(ptr != nullptr);
      auto& ref = dynamic_cast<newb<message>&>(*ptr);
      accept_pol->init(this, ref);
    }
  }

  void write_event() {
    accept_pol->write_event(this);
  }

  virtual expected<actor> create_newb(network::native_socket sockfd,
                                      policy::transport_ptr pol) {
    CAF_LOG_TRACE(CAF_ARG(sockfd));
    auto n = detail::apply_args_prefixed(
      io::spawn_newb<Protocol, no_spawn_options, Fun, Ts...>,
      detail::get_indices(args_),
      args_, this->backend().system(),
      fun_, std::move(pol), sockfd
    );
    return n;
  }

  policy::accept_ptr<message_type> accept_pol;

private:
  Fun fun_;
  bool reading_;
  bool writing_;
  std::tuple<Ts...> args_;
};

template <class P, class F, class... Ts>
using acceptor_ptr = caf::intrusive_ptr<newb_acceptor<P, F, Ts...>>;

template <class Protocol, class Fun, class Message, class... Ts>
acceptor_ptr<Protocol, Fun, Ts...>
make_acceptor(actor_system& sys, Fun fun, policy::accept_ptr<Message> pol,
              network::native_socket sockfd, Ts&&... xs) {
  auto& dm = dynamic_cast<network::default_multiplexer&>(sys.middleman().backend());
  auto res = make_counted<newb_acceptor<Protocol, Fun, Ts...>>(dm, sockfd,
                                                               std::move(fun),
                                                               std::move(pol),
                                                               std::forward<Ts>(xs)...);
  res->start();
  return res;
}

template <class Protocol, class F, class Message, class... Ts>
expected<caf::intrusive_ptr<newb_acceptor<Protocol, F, Ts...>>>
make_server(actor_system& sys, F fun, policy::accept_ptr<Message> pol,
            uint16_t port, const char* addr, bool reuse, Ts&&... xs) {
  auto esock = pol->create_socket(port, addr, reuse);
  if (!esock) {
    CAF_LOG_ERROR("Could not open " << CAF_ARG(port) << CAF_ARG(addr));
    return sec::cannot_open_port;
  }
  return make_acceptor<Protocol, F>(sys, std::move(fun), std::move(pol), *esock,
                                    std::forward<Ts>(xs)...);
}

template <class Protocol, class F, class Message>
expected<caf::intrusive_ptr<newb_acceptor<Protocol, F>>>
make_server(actor_system& sys, F fun, policy::accept_ptr<Message> pol,
            uint16_t port, const char* addr = nullptr, bool reuse = false) {
 return make_server<Protocol, F>(sys, std::move(fun), std::move(pol), port,
                                 addr, reuse);
}

} // namespace io
} // namespace caf

