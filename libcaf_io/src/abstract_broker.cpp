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

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace io {

class abstract_broker::continuation {
public:
  continuation(intrusive_ptr<abstract_broker> bptr, mailbox_element_ptr mptr)
      : self_(std::move(bptr)),
        ptr_(std::move(mptr)) {
    // nop
  }

  continuation(continuation&&) = default;

  inline void operator()() {
    CAF_PUSH_AID(self_->id());
    CAF_LOG_TRACE("");
    self_->invoke_message(ptr_);
  }

private:
  intrusive_ptr<abstract_broker> self_;
  mailbox_element_ptr ptr_;
};

void abstract_broker::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  backend().post(continuation{this, std::move(ptr)});
}

void abstract_broker::enqueue(const actor_addr& sender, message_id mid,
                              message msg, execution_unit* eu) {
  enqueue(mailbox_element::make(sender, mid, std::move(msg)), eu);
}

void abstract_broker::launch(execution_unit*, bool, bool is_hidden) {
  // add implicit reference count held by the middleman
  ref();
  is_registered(! is_hidden);
  CAF_PUSH_AID(id());
  CAF_LOGF_TRACE("init and launch broker with ID " << id());
  // we want to make sure initialization is executed in MM context
  do_become(
    [=](sys_atom) {
      CAF_LOGF_TRACE("ID " << id());
      bhvr_stack_.pop_back();
      // launch backends now, because user-defined initialization
      // might call functions like add_connection
      for (auto& kvp : doormen_) {
        kvp.second->launch();
      }
      initialize();
    },
    true);
  enqueue(invalid_actor_addr, invalid_message_id,
          make_message(sys_atom::value), nullptr);
}

void abstract_broker::cleanup(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  planned_exit_reason(reason);
  on_exit();
  close_all();
  CAF_ASSERT(doormen_.empty());
  CAF_ASSERT(scribes_.empty());
  CAF_ASSERT(current_mailbox_element() == nullptr);
  cache_.clear();
  local_actor::cleanup(reason);
  deref(); // release implicit reference count from middleman
}

void abstract_broker::servant::set_broker(abstract_broker* new_broker) {
  if (! disconnected_) {
    broker_ = new_broker;
  }
}

abstract_broker::servant::~servant() {
  CAF_LOG_TRACE("");
}

abstract_broker::servant::servant(abstract_broker* ptr) : disconnected_(false), broker_(ptr) {
  // nop
}

void abstract_broker::servant::disconnect(bool invoke_disconnect_message) {
  CAF_LOG_TRACE("");
  if (! disconnected_) {
    CAF_LOG_DEBUG("disconnect servant from broker");
    disconnected_ = true;
    remove_from_broker();
    if (invoke_disconnect_message) {
      auto msg = disconnect_message();
      broker_->invoke_message(broker_->address(),invalid_message_id, msg);
    }
  }
}

abstract_broker::scribe::scribe(abstract_broker* ptr, connection_handle conn_hdl)
    : servant(ptr),
      hdl_(conn_hdl) {
  std::vector<char> tmp;
  read_msg_ = make_message(new_data_msg{hdl_, std::move(tmp)});
}

void abstract_broker::scribe::remove_from_broker() {
  CAF_LOG_TRACE("hdl = " << hdl().id());
  broker_->scribes_.erase(hdl());
}

abstract_broker::scribe::~scribe() {
  CAF_LOG_TRACE("");
}

message abstract_broker::scribe::disconnect_message() {
  return make_message(connection_closed_msg{hdl()});
}

void abstract_broker::scribe::consume(const void*, size_t num_bytes) {
  CAF_LOG_TRACE(CAF_ARG(num_bytes));
  if (disconnected_) {
    // we are already disconnected from the broker while the multiplexer
    // did not yet remove the socket, this can happen if an IO event causes
    // the broker to call close_all() while the pollset contained
    // further activities for the broker
    return;
  }
  auto& buf = rd_buf();
  buf.resize(num_bytes);                       // make sure size is correct
  read_msg().buf.swap(buf);                    // swap into message
  broker_->invoke_message(invalid_actor_addr, // call client
                           invalid_message_id, read_msg_);
  read_msg().buf.swap(buf); // swap buffer back to stream
  flush();                  // implicit flush of wr_buf()
}

void abstract_broker::scribe::io_failure(network::operation op) {
  CAF_LOG_TRACE("id = " << hdl().id()
                << ", " << CAF_TARG(op, static_cast<int>));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  disconnect(true);
}

abstract_broker::doorman::doorman(abstract_broker* ptr, accept_handle acc_hdl)
    : servant(ptr), hdl_(acc_hdl) {
  auto hdl2 = connection_handle::from_int(-1);
  accept_msg_ = make_message(new_connection_msg{hdl_, hdl2});
}

abstract_broker::doorman::~doorman() {
  CAF_LOG_TRACE("");
}

void abstract_broker::doorman::remove_from_broker() {
  CAF_LOG_TRACE("hdl = " << hdl().id());
  broker_->doormen_.erase(hdl());
}

message abstract_broker::doorman::disconnect_message() {
  return make_message(acceptor_closed_msg{hdl()});
}

void abstract_broker::doorman::io_failure(network::operation op) {
  CAF_LOG_TRACE("id = " << hdl().id() << ", "
                        << CAF_TARG(op, static_cast<int>));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  disconnect(true);
}

abstract_broker::~abstract_broker() {
  CAF_LOG_TRACE("");
}

void abstract_broker::configure_read(connection_handle hdl, receive_policy::config cfg) {
  CAF_LOG_TRACE(CAF_MARG(hdl, id) << ", cfg = {" << static_cast<int>(cfg.first)
                                  << ", " << cfg.second << "}");
  by_id(hdl).configure_read(cfg);
}

abstract_broker::buffer_type& abstract_broker::wr_buf(connection_handle hdl) {
  return by_id(hdl).wr_buf();
}

void abstract_broker::write(connection_handle hdl, size_t bs, const void* buf) {
  auto& out = wr_buf(hdl);
  auto first = reinterpret_cast<const char*>(buf);
  auto last = first + bs;
  out.insert(out.end(), first, last);
}

void abstract_broker::flush(connection_handle hdl) {
  by_id(hdl).flush();
}

std::vector<connection_handle> abstract_broker::connections() const {
  std::vector<connection_handle> result;
  result.reserve(scribes_.size());
  for (auto& kvp : scribes_) {
    result.push_back(kvp.first);
  }
  return result;
}

connection_handle abstract_broker::add_tcp_scribe(const std::string& hostname,
                                                  uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(hostname) << ", " << CAF_ARG(port));
  return backend().add_tcp_scribe(this, hostname, port);
}

void abstract_broker::assign_tcp_scribe(connection_handle hdl) {
  CAF_LOG_TRACE(CAF_MARG(hdl, id));
  backend().assign_tcp_scribe(this, hdl);
}

connection_handle
abstract_broker::add_tcp_scribe(network::native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  return backend().add_tcp_scribe(this, fd);
}

std::pair<accept_handle, uint16_t>
abstract_broker::add_tcp_doorman(uint16_t port, const char* in,
                                 bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << ", in = " << (in ? in : "nullptr")
                << ", " << CAF_ARG(reuse_addr));
  return backend().add_tcp_doorman(this, port, in, reuse_addr);
}

void abstract_broker::assign_tcp_doorman(accept_handle hdl) {
  CAF_LOG_TRACE(CAF_MARG(hdl, id));
  backend().assign_tcp_doorman(this, hdl);
}

accept_handle abstract_broker::add_tcp_doorman(network::native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  return backend().add_tcp_doorman(this, fd);
}

void abstract_broker::invoke_message(mailbox_element_ptr& ptr) {
  CAF_LOG_TRACE(CAF_TARG(ptr->msg, to_string));
  if (exit_reason() != exit_reason::not_exited || ! has_behavior()) {
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
          cache_.push_second_back(ptr.release());
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
  } else if (! has_behavior()) {
    CAF_LOG_DEBUG("no behavior set, quit for normal exit reason");
    quit(exit_reason::normal);
    cleanup(planned_exit_reason());
  }
}

void abstract_broker::invoke_message(const actor_addr& sender,
                            message_id mid, message& msg) {
  auto ptr = mailbox_element::make(sender, mid, message{});
  ptr->msg.swap(msg);
  invoke_message(ptr);
  if (ptr) {
    ptr->msg.swap(msg);
  }
}

void abstract_broker::close_all() {
  CAF_LOG_TRACE("");
  while (! doormen_.empty()) {
    // stop_reading will remove the doorman from doormen_
    doormen_.begin()->second->stop_reading();
  }
  while (! scribes_.empty()) {
    // stop_reading will remove the scribe from scribes_
    scribes_.begin()->second->stop_reading();
  }
}

void abstract_broker::close(connection_handle hdl) {
  by_id(hdl).stop_reading();
}

void abstract_broker::close(accept_handle hdl) {
  by_id(hdl).stop_reading();
}

bool abstract_broker::valid(connection_handle hdl) {
  return scribes_.count(hdl) > 0;
}

bool abstract_broker::valid(accept_handle hdl) {
  return doormen_.count(hdl) > 0;
}

abstract_broker::abstract_broker() : mm_(*middleman::instance()) {
  // nop
}

abstract_broker::abstract_broker(middleman& ptr) : mm_(ptr) {
  // nop
}

network::multiplexer& abstract_broker::backend() {
  return mm_.backend();
}

bool abstract_broker::invoke_message_from_cache() {
  CAF_LOG_TRACE("");
  auto& bhvr = this->awaits_response()
               ? this->awaited_response_handler()
               : this->bhvr_stack().back();
  auto bid = this->awaited_response_id();
  auto i = cache_.second_begin();
  auto e = cache_.second_end();
  CAF_LOG_DEBUG(std::distance(i, e) << " elements in cache");
  return cache_.invoke(static_cast<local_actor*>(this), i, e, bhvr, bid);
}

} // namespace io
} // namespace caf
