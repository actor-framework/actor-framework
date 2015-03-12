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

#include "caf/none.hpp"
#include "caf/config.hpp"
#include "caf/make_counted.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/scope_guard.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#include "caf/detail/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace io {

broker::servant::~servant() {
  CAF_LOG_TRACE("");
}

broker::servant::servant(broker* ptr) : m_disconnected(false), m_broker(ptr) {
  // nop
}

void broker::servant::set_broker(broker* new_broker) {
  if (!m_disconnected) {
    m_broker = new_broker;
  }
}

void broker::servant::disconnect(bool invoke_disconnect_message) {
  CAF_LOG_TRACE("");
  if (!m_disconnected) {
    CAF_LOG_DEBUG("disconnect servant from broker");
    m_disconnected = true;
    remove_from_broker();
    if (invoke_disconnect_message) {
      auto msg = disconnect_message();
      m_broker->invoke_message(m_broker->address(),invalid_message_id, msg);
    }
  }
}

broker::scribe::scribe(broker* ptr, connection_handle conn_hdl)
    : servant(ptr),
      m_hdl(conn_hdl) {
  std::vector<char> tmp;
  m_read_msg = make_message(new_data_msg{m_hdl, std::move(tmp)});
}

void broker::scribe::remove_from_broker() {
  CAF_LOG_TRACE("hdl = " << hdl().id());
  m_broker->m_scribes.erase(hdl());
}

broker::scribe::~scribe() {
  CAF_LOG_TRACE("");
}

message broker::scribe::disconnect_message() {
  return make_message(connection_closed_msg{hdl()});
}

void broker::scribe::consume(const void*, size_t num_bytes) {
  CAF_LOG_TRACE(CAF_ARG(num_bytes));
  if (m_disconnected) {
    // we are already disconnected from the broker while the multiplexer
    // did not yet remove the socket, this can happen if an IO event causes
    // the broker to call close_all() while the pollset contained
    // further activities for the broker
    return;
  }
  auto& buf = rd_buf();
  buf.resize(num_bytes);                       // make sure size is correct
  read_msg().buf.swap(buf);                    // swap into message
  m_broker->invoke_message(invalid_actor_addr, // call client
                           invalid_message_id, m_read_msg);
  read_msg().buf.swap(buf); // swap buffer back to stream
  flush();                  // implicit flush of wr_buf()
}

void broker::scribe::io_failure(network::operation op) {
  CAF_LOG_TRACE("id = " << hdl().id()
                << ", " << CAF_TARG(op, static_cast<int>));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  disconnect(true);
}

broker::doorman::doorman(broker* ptr, accept_handle acc_hdl)
    : servant(ptr), m_hdl(acc_hdl) {
  auto hdl2 = connection_handle::from_int(-1);
  m_accept_msg = make_message(new_connection_msg{m_hdl, hdl2});
}

broker::doorman::~doorman() {
  CAF_LOG_TRACE("");
}

void broker::doorman::remove_from_broker() {
  CAF_LOG_TRACE("hdl = " << hdl().id());
  m_broker->m_doormen.erase(hdl());
}

message broker::doorman::disconnect_message() {
  return make_message(acceptor_closed_msg{hdl()});
}

void broker::doorman::io_failure(network::operation op) {
  CAF_LOG_TRACE("id = " << hdl().id() << ", "
                        << CAF_TARG(op, static_cast<int>));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  disconnect(true);
}

class broker::continuation {
 public:
  continuation(broker_ptr bptr, mailbox_element_ptr mptr)
      : m_self(std::move(bptr)),
        m_ptr(std::move(mptr)) {
    // nop
  }

  continuation(continuation&&) = default;

  inline void operator()() {
    CAF_PUSH_AID(m_self->id());
    CAF_LOG_TRACE("");
    m_self->invoke_message(m_ptr);
  }

 private:
  broker_ptr m_self;
  mailbox_element_ptr m_ptr;
};

void broker::invoke_message(mailbox_element_ptr& ptr) {
  CAF_LOG_TRACE(CAF_TARG(ptr->msg, to_string));
  if (exit_reason() != exit_reason::not_exited || !has_behavior()) {
    CAF_LOG_DEBUG("actor already finished execution"
                  << ", planned_exit_reason = " << planned_exit_reason()
                  << ", has_behavior() = " << has_behavior());
    if (ptr->mid.valid()) {
      detail::sync_request_bouncer srb{exit_reason()};
      srb(ptr->sender, ptr->mid);
    }
    return;
  }
  // prepare actor for invocation of message handler
  try {
    auto& bhvr = this->awaits_response()
                 ? this->awaited_response_handler()
                 : this->bhvr_stack().back();
    auto bid = this->awaited_response_id();
    switch (local_actor::invoke_message(ptr, bhvr, bid)) {
      case im_success: {
        CAF_LOG_DEBUG("handle_message returned hm_msg_handled");
        while (has_behavior()
               && planned_exit_reason() == exit_reason::not_exited
               && invoke_message_from_cache()) {
          // rinse and repeat
        }
        break;
      }
      case im_dropped:
        CAF_LOG_DEBUG("handle_message returned hm_drop_msg");
        break;
      case im_skipped: {
        CAF_LOG_DEBUG("handle_message returned hm_skip_msg or hm_cache_msg");
        if (ptr) {
          m_cache.push_second_back(ptr.release());
        }
        break;
      }
    }
  }
  catch (std::exception& e) {
    CAF_LOG_INFO("broker killed due to an unhandled exception: "
                 << to_verbose_string(e));
    // keep compiler happy in non-debug mode
    static_cast<void>(e);
    quit(exit_reason::unhandled_exception);
  }
  catch (...) {
    CAF_LOG_ERROR("broker killed due to an unknown exception");
    quit(exit_reason::unhandled_exception);
  }
  // safe to actually release behaviors now
  bhvr_stack().cleanup();
  // cleanup actor if needed
  if (planned_exit_reason() != exit_reason::not_exited) {
    cleanup(planned_exit_reason());
  } else if (!has_behavior()) {
    CAF_LOG_DEBUG("no behavior set, quit for normal exit reason");
    quit(exit_reason::normal);
    cleanup(planned_exit_reason());
  }
}

void broker::invoke_message(const actor_addr& v0, message_id v1, message& v2) {
  auto ptr = mailbox_element::make(v0, v1, message{});
  ptr->msg.swap(v2);
  invoke_message(ptr);
  if (ptr) {
    ptr->msg.swap(v2);
  }
}

bool broker::invoke_message_from_cache() {
  CAF_LOG_TRACE("");
  auto& bhvr = this->awaits_response()
               ? this->awaited_response_handler()
               : this->bhvr_stack().back();
  auto bid = this->awaited_response_id();
  auto i = m_cache.second_begin();
  auto e = m_cache.second_end();
  CAF_LOG_DEBUG(std::distance(i, e) << " elements in cache");
  return m_cache.invoke(static_cast<local_actor*>(this), i, e, bhvr, bid);
}

void broker::write(connection_handle hdl, size_t bs, const void* buf) {
  auto& out = wr_buf(hdl);
  auto first = reinterpret_cast<const char*>(buf);
  auto last = first + bs;
  out.insert(out.end(), first, last);
}

void broker::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  parent().backend().post(continuation{this, std::move(ptr)});
}

void broker::enqueue(const actor_addr& sender, message_id mid, message msg,
                     execution_unit* eu) {
  enqueue(mailbox_element::make(sender, mid, std::move(msg)), eu);
}

broker::broker() : m_mm(*middleman::instance()) {
  // nop
}

broker::broker(middleman& ptr) : m_mm(ptr) {
  // nop
}

void broker::cleanup(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  planned_exit_reason(reason);
  on_exit();
  close_all();
  CAF_REQUIRE(m_doormen.empty());
  CAF_REQUIRE(m_scribes.empty());
  CAF_REQUIRE(current_mailbox_element() == nullptr);
  m_cache.clear();
  super::cleanup(reason);
  deref(); // release implicit reference count from middleman
}

void broker::on_exit() {
  // nop
}

void broker::launch(execution_unit*, bool, bool is_hidden) {
  // add implicit reference count held by the middleman
  ref();
  is_registered(!is_hidden);
  CAF_PUSH_AID(id());
  CAF_LOGF_TRACE("init and launch broker with ID " << id());
  // we want to make sure initialization is executed in MM context
  become(
    on(atom("INITMSG")) >> [=] {
      CAF_LOGF_TRACE("ID " << id());
      unbecome();
      // launch backends now, because user-defined initialization
      // might call functions like add_connection
      for (auto& kvp : m_doormen) {
        kvp.second->launch();
      }
      is_initialized(true);
      // run user-defined initialization code
      auto bhvr = make_behavior();
      if (bhvr) {
        become(std::move(bhvr));
      }
    }
  );
  enqueue(invalid_actor_addr, invalid_message_id,
          make_message(atom("INITMSG")), nullptr);
}

void broker::configure_read(connection_handle hdl, receive_policy::config cfg) {
  CAF_LOG_TRACE(CAF_MARG(hdl, id) << ", cfg = {" << static_cast<int>(cfg.first)
                                  << ", " << cfg.second << "}");
  by_id(hdl).configure_read(cfg);
}

void broker::flush(connection_handle hdl) {
  by_id(hdl).flush();
}

broker::buffer_type& broker::wr_buf(connection_handle hdl) {
  return by_id(hdl).wr_buf();
}

broker::~broker() {
  CAF_LOG_TRACE("");
}

void broker::close(connection_handle hdl) {
  by_id(hdl).stop_reading();
}

void broker::close(accept_handle hdl) {
  by_id(hdl).stop_reading();
}

void broker::close_all() {
  CAF_LOG_TRACE("");
  while (!m_doormen.empty()) {
    // stop_reading will remove the doorman from m_doormen
    m_doormen.begin()->second->stop_reading();
  }
  while (!m_scribes.empty()) {
    // stop_reading will remove the scribe from m_scribes
    m_scribes.begin()->second->stop_reading();
  }
}

bool broker::valid(connection_handle hdl) {
  return m_scribes.count(hdl) > 0;
}

bool broker::valid(accept_handle hdl) {
  return m_doormen.count(hdl) > 0;
}

std::vector<connection_handle> broker::connections() const {
  std::vector<connection_handle> result;
  for (auto& kvp : m_scribes) {
    result.push_back(kvp.first);
  }
  return result;
}

void broker::initialize() {
  // nop
}

broker::functor_based::~functor_based() {
  // nop
}

behavior broker::functor_based::make_behavior() {
  return m_make_behavior(this);
}

network::multiplexer& broker::backend() {
  return m_mm.backend();
}

connection_handle broker::add_tcp_scribe(const std::string& hst, uint16_t prt) {
  CAF_LOG_TRACE(CAF_ARG(hst) << ", " << CAF_ARG(prt));
  return backend().add_tcp_scribe(this, hst, prt);
}

void broker::assign_tcp_scribe(connection_handle hdl) {
  CAF_LOG_TRACE(CAF_MARG(hdl, id));
  backend().assign_tcp_scribe(this, hdl);
}

connection_handle broker::add_tcp_scribe(network::native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  return backend().add_tcp_scribe(this, fd);
}

void broker::assign_tcp_doorman(accept_handle hdl) {
  CAF_LOG_TRACE(CAF_MARG(hdl, id));
  backend().assign_tcp_doorman(this, hdl);
}

std::pair<accept_handle, uint16_t>
broker::add_tcp_doorman(uint16_t port, const char* in, bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << ", in = " << (in ? in : "nullptr")
                << ", " << CAF_ARG(reuse_addr));
  return backend().add_tcp_doorman(this, port, in, reuse_addr);
}

accept_handle broker::add_tcp_doorman(network::native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  return backend().add_tcp_doorman(this, fd);
}

} // namespace io
} // namespace caf
