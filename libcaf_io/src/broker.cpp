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

#include "caf/none.hpp"

#include "caf/config.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/scope_guard.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#include "caf/detail/make_counted.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace io {

broker::servant::~servant() {
  // nop
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

broker::scribe::scribe(broker* parent, connection_handle hdl)
    : servant(parent), m_hdl(hdl) {
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

broker::doorman::doorman(broker* parent, accept_handle hdl)
    : servant(parent), m_hdl(hdl) {
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
  continuation(broker_ptr ptr, actor_addr from, message_id mid, message&& msg)
      : m_self(std::move(ptr)),
        m_from(from),
        m_mid(mid),
        m_data(std::move(msg)) {
    // nop
  }

  inline void operator()() {
    CAF_PUSH_AID(m_self->id());
    CAF_LOG_TRACE("");
    m_self->invoke_message(m_from, m_mid, m_data);
  }

 private:
  broker_ptr m_self;
  actor_addr m_from;
  message_id m_mid;
  message m_data;
};

void broker::invoke_message(const actor_addr& sender, message_id mid,
                            message& msg) {
  CAF_LOG_TRACE(CAF_TARG(msg, to_string));
  if (planned_exit_reason() != exit_reason::not_exited
      || bhvr_stack().empty()) {
    CAF_LOG_DEBUG("actor already finished execution"
                  << ", planned_exit_reason = " << planned_exit_reason()
                  << ", bhvr_stack().empty() = " << bhvr_stack().empty());
    if (mid.valid()) {
      detail::sync_request_bouncer srb{exit_reason()};
      srb(sender, mid);
    }
    return;
  }
  // prepare actor for invocation of message handler
  m_dummy_node.sender = sender;
  m_dummy_node.mid = mid;
  std::swap(msg, m_dummy_node.msg);
  try {
    auto bhvr = bhvr_stack().back();
    auto mid = bhvr_stack().back_id();
    switch (m_invoke_policy.handle_message(this, &m_dummy_node, bhvr, mid)) {
      case policy::hm_msg_handled: {
        CAF_LOG_DEBUG("handle_message returned hm_msg_handled");
        while (!bhvr_stack().empty()
               && planned_exit_reason() == exit_reason::not_exited
               && invoke_message_from_cache()) {
          // rinse and repeat
        }
        break;
      }
      case policy::hm_drop_msg:
        CAF_LOG_DEBUG("handle_message returned hm_drop_msg");
        break;
      case policy::hm_skip_msg:
      case policy::hm_cache_msg: {
        CAF_LOG_DEBUG("handle_message returned hm_skip_msg or hm_cache_msg");
        auto e = mailbox_element::create(sender, mid,
                                         std::move(m_dummy_node.msg));
        m_priority_policy.push_to_cache(unique_mailbox_element_pointer{e});
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
  // restore dummy node
  m_dummy_node.sender = invalid_actor_addr;
  std::swap(m_dummy_node.msg, msg);
  // cleanup if needed
  if (planned_exit_reason() != exit_reason::not_exited) {
    cleanup(planned_exit_reason());
    // release implicit reference count held by MM
    // deref();
  } else if (bhvr_stack().empty()) {
    CAF_LOG_DEBUG("bhvr_stack().empty(), quit for normal exit reason");
    quit(exit_reason::normal);
    cleanup(planned_exit_reason());
    // release implicit reference count held by MM
    // deref();
  }
}

bool broker::invoke_message_from_cache() {
  CAF_LOG_TRACE("");
  auto bhvr = bhvr_stack().back();
  auto mid = bhvr_stack().back_id();
  auto e = m_priority_policy.cache_end();
  CAF_LOG_DEBUG(std::distance(m_priority_policy.cache_begin(), e)
                << " elements in cache");
  for (auto i = m_priority_policy.cache_begin(); i != e; ++i) {
    auto res = m_invoke_policy.invoke_message(this, *i, bhvr, mid);
    if (res || !*i) {
      m_priority_policy.cache_erase(i);
      if (res){
        return true;
      }
      return invoke_message_from_cache();
    }
  }
  return false;
}

void broker::write(connection_handle hdl, size_t bs, const void* buf) {
  auto& out = wr_buf(hdl);
  auto first = reinterpret_cast<const char*>(buf);
  auto last = first + bs;
  out.insert(out.end(), first, last);
}

void broker::enqueue(const actor_addr& sender, message_id mid, message msg,
                     execution_unit*) {
  parent().backend().post(continuation{this, sender,
                                       mid, std::move(msg)});
}

broker::broker() : m_mm(*middleman::instance()) {
  // nop
}

void broker::cleanup(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  close_all();
  super::cleanup(reason);
  deref(); // release implicit reference count from middleman
}

void broker::launch(bool is_hidden, execution_unit*) {
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

std::vector<connection_handle> broker::connections() const {
  std::vector<connection_handle> result;
  for (auto& scribe : m_scribes) {
    result.push_back(scribe.first);
  }
  return result;
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

} // namespace io
} // namespace caf
