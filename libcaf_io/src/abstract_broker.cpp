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

#include "caf/event_based_actor.hpp"

namespace caf {
namespace io {

class abstract_broker::continuation {
public:
  continuation(intrusive_ptr<abstract_broker> bptr) : self_(std::move(bptr)) {
    // nop
  }

  continuation(continuation&&) = default;

  inline void operator()() {
    CAF_PUSH_AID(self_->id());
    CAF_LOG_TRACE("");
    auto mt = self_->parent().max_throughput();
    // re-schedule broker if it reached its maximum message throughput
    if (self_->resume(nullptr, mt) == resumable::resume_later)
      self_->backend().post(std::move(*this));
  }

private:
  intrusive_ptr<abstract_broker> self_;
};

void abstract_broker::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  CAF_PUSH_AID(id());
  CAF_LOG_TRACE("enqueue " << CAF_TSARG(ptr->msg));
  auto mid = ptr->mid;
  auto sender = ptr->sender;
  switch (mailbox().enqueue(ptr.release())) {
    case detail::enqueue_result::unblocked_reader: {
      // re-schedule broker
      CAF_LOG_DEBUG("unblocked_reader");
      backend().post(continuation{this});
      break;
    }
    case detail::enqueue_result::queue_closed: {
      CAF_LOG_DEBUG("queue_closed");
      if (mid.is_request()) {
        detail::sync_request_bouncer f{exit_reason()};
        f(sender, mid);
      }
      break;
    }
    case detail::enqueue_result::success:
      // enqueued to a running actors' mailbox; nothing to do
      CAF_LOG_DEBUG("success");
      break;
  }
}

void abstract_broker::enqueue(const actor_addr& sender, message_id mid,
                              message msg, execution_unit* eu) {
  enqueue(mailbox_element::make(sender, mid, std::move(msg)), eu);
}

void abstract_broker::launch(execution_unit*, bool is_lazy, bool is_hidden) {
  // add implicit reference count held by the middleman
  ref();
  is_registered(! is_hidden);
  CAF_PUSH_AID(id());
  CAF_LOGF_TRACE("init and launch broker with ID " << id());
  if (is_lazy && mailbox().try_block())
    return;
  backend().post(continuation{this});
}

void abstract_broker::cleanup(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  close_all();
  CAF_ASSERT(doormen_.empty());
  CAF_ASSERT(scribes_.empty());
  cache_.clear();
  local_actor::cleanup(reason);
  deref(); // release implicit reference count from middleman
}

abstract_broker::~abstract_broker() {
  // nop
}

void abstract_broker::configure_read(connection_handle hdl,
                                     receive_policy::config cfg) {
  CAF_LOG_TRACE(CAF_MARG(hdl, id) << ", cfg = {" << static_cast<int>(cfg.first)
                                  << ", " << cfg.second << "}");
  by_id(hdl).configure_read(cfg);
}

std::vector<char>& abstract_broker::wr_buf(connection_handle hdl) {
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

void abstract_broker::add_scribe(const intrusive_ptr<scribe>& ptr) {
  scribes_.emplace(ptr->hdl(), ptr);
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

void abstract_broker::add_doorman(const intrusive_ptr<doorman>& ptr) {
  doormen_.emplace(ptr->hdl(), ptr);
  if (is_initialized())
    ptr->launch();
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

std::string abstract_broker::remote_addr(connection_handle hdl) {
  auto i = scribes_.find(hdl);
  return i != scribes_.end() ? i->second->addr() : std::string{};
}

uint16_t abstract_broker::remote_port(connection_handle hdl) {
  auto i = scribes_.find(hdl);
  return i != scribes_.end() ? i->second->port() : 0;
}

std::string abstract_broker::local_addr(accept_handle hdl) {
  auto i = doormen_.find(hdl);
  return i != doormen_.end() ? i->second->addr() : std::string{};
}

uint16_t abstract_broker::local_port(accept_handle hdl) {
  auto i = doormen_.find(hdl);
  return i != doormen_.end() ? i->second->port() : 0;
}

maybe<accept_handle> abstract_broker::hdl_by_port(uint16_t port) {
  for (auto& kvp : doormen_)
    if (kvp.second->port() == port)
      return kvp.first;
  return none;
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

const char* abstract_broker::name() const {
  return "broker";
}

void abstract_broker::init_broker() {
  CAF_LOG_TRACE("");
  is_initialized(true);
  // launch backends now, because user-defined initialization
  // might call functions like add_connection
  for (auto& kvp : doormen_)
    kvp.second->launch();

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

} // namespace io
} // namespace caf
