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
#include "caf/logger.hpp"
#include "caf/actor_system.hpp"
#include "caf/make_counted.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"

#include "caf/detail/scope_guard.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/event_based_actor.hpp"

namespace caf {
namespace io {

void abstract_broker::enqueue(strong_actor_ptr src, message_id mid,
                              message msg, execution_unit*) {
  enqueue(make_mailbox_element(std::move(src), mid, {}, std::move(msg)),
          &backend());
}

void abstract_broker::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  CAF_PUSH_AID(id());
  scheduled_actor::enqueue(std::move(ptr), &backend());
}

void abstract_broker::launch(execution_unit* eu, bool is_lazy, bool is_hidden) {
  CAF_ASSERT(eu != nullptr);
  CAF_ASSERT(eu == &backend());
  // add implicit reference count held by middleman/multiplexer
  is_registered(! is_hidden);
  CAF_PUSH_AID(id());
  CAF_LOG_TRACE("init and launch broker:" << CAF_ARG(id()));
  if (is_lazy && mailbox().try_block())
    return;
  intrusive_ptr_add_ref(ctrl());
  eu->exec_later(this);
}

bool abstract_broker::cleanup(error&& reason, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  close_all();
  CAF_ASSERT(doormen_.empty());
  CAF_ASSERT(scribes_.empty());
  cache_.clear();
  return local_actor::cleanup(std::move(reason), host);
}

abstract_broker::~abstract_broker() {
  // nop
}

void abstract_broker::configure_read(connection_handle hdl,
                                     receive_policy::config cfg) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(cfg));
  auto x = by_id(hdl);
  if (x)
    x->configure_read(cfg);
}

void abstract_broker::ack_writes(connection_handle hdl, bool enable) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(enable));
  auto x = by_id(hdl);
  if (x)
    x->ack_writes(enable);
}

std::vector<char>& abstract_broker::wr_buf(connection_handle hdl) {
  auto x = by_id(hdl);
  if (! x) {
    CAF_LOG_ERROR("tried to access wr_buf() of an unknown connection_handle");
    return dummy_wr_buf_;
  }
  return x->wr_buf();
}

void abstract_broker::write(connection_handle hdl, size_t bs, const void* buf) {
  auto& out = wr_buf(hdl);
  auto first = reinterpret_cast<const char*>(buf);
  auto last = first + bs;
  out.insert(out.end(), first, last);
}

void abstract_broker::flush(connection_handle hdl) {
  auto x = by_id(hdl);
  if (x)
    x->flush();
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

expected<connection_handle> abstract_broker::add_tcp_scribe(const std::string& hostname,
                                                  uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(hostname) << ", " << CAF_ARG(port));
  return backend().add_tcp_scribe(this, hostname, port);
}

expected<void> abstract_broker::assign_tcp_scribe(connection_handle hdl) {
  CAF_LOG_TRACE(CAF_ARG(hdl));
  return backend().assign_tcp_scribe(this, hdl);
}

expected<connection_handle>
abstract_broker::add_tcp_scribe(network::native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  return backend().add_tcp_scribe(this, fd);
}

void abstract_broker::add_doorman(const intrusive_ptr<doorman>& ptr) {
  doormen_.emplace(ptr->hdl(), ptr);
  if (is_initialized())
    ptr->launch();
}

expected<std::pair<accept_handle, uint16_t>>
abstract_broker::add_tcp_doorman(uint16_t port, const char* in,
                                 bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(in) << CAF_ARG(reuse_addr));
  return backend().add_tcp_doorman(this, port, in, reuse_addr);
}

expected<void> abstract_broker::assign_tcp_doorman(accept_handle hdl) {
  CAF_LOG_TRACE(CAF_ARG(hdl));
  return backend().assign_tcp_doorman(this, hdl);
}

expected<accept_handle> abstract_broker::add_tcp_doorman(network::native_socket fd) {
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

accept_handle abstract_broker::hdl_by_port(uint16_t port) {
  for (auto& kvp : doormen_)
    if (kvp.second->port() == port)
      return kvp.first;
  return invalid_accept_handle;
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

resumable::subtype_t abstract_broker::subtype() const {
  return io_actor;
}

resumable::resume_result
abstract_broker::resume(execution_unit* ctx, size_t mt) {
  CAF_ASSERT(ctx != nullptr);
  CAF_ASSERT(ctx == &backend());
  return scheduled_actor::resume(ctx, mt);
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

abstract_broker::abstract_broker(actor_config& cfg) : scheduled_actor(cfg) {
  // nop
}

network::multiplexer& abstract_broker::backend() {
  return system().middleman().backend();
}

} // namespace io
} // namespace caf
