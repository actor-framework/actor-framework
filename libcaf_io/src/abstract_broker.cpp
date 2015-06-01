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

namespace caf {
namespace io {

void abstract_broker::servant::set_broker(abstract_broker* new_broker) {
  if (!m_disconnected) {
    m_broker = new_broker;
  }
}

abstract_broker::servant::~servant() {
  CAF_LOG_TRACE("");
}

abstract_broker::servant::servant(abstract_broker* ptr) : m_disconnected(false), m_broker(ptr) {
  // nop
}

void abstract_broker::servant::disconnect(bool invoke_disconnect_message) {
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

abstract_broker::scribe::scribe(abstract_broker* ptr, connection_handle conn_hdl)
    : servant(ptr),
      m_hdl(conn_hdl) {
  std::vector<char> tmp;
  m_read_msg = make_message(new_data_msg{m_hdl, std::move(tmp)});
}

void abstract_broker::scribe::remove_from_broker() {
  CAF_LOG_TRACE("hdl = " << hdl().id());
  m_broker->m_scribes.erase(hdl());
}

abstract_broker::scribe::~scribe() {
  CAF_LOG_TRACE("");
}

message abstract_broker::scribe::disconnect_message() {
  return make_message(connection_closed_msg{hdl()});
}

void abstract_broker::scribe::consume(const void*, size_t num_bytes) {
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

void abstract_broker::scribe::io_failure(network::operation op) {
  CAF_LOG_TRACE("id = " << hdl().id()
                << ", " << CAF_TARG(op, static_cast<int>));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  disconnect(true);
}

abstract_broker::doorman::doorman(abstract_broker* ptr, accept_handle acc_hdl)
    : servant(ptr), m_hdl(acc_hdl) {
  auto hdl2 = connection_handle::from_int(-1);
  m_accept_msg = make_message(new_connection_msg{m_hdl, hdl2});
}

abstract_broker::doorman::~doorman() {
  CAF_LOG_TRACE("");
}

void abstract_broker::doorman::remove_from_broker() {
  CAF_LOG_TRACE("hdl = " << hdl().id());
  m_broker->m_doormen.erase(hdl());
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
  result.reserve(m_scribes.size());
  for (auto& kvp : m_scribes) {
    result.push_back(kvp.first);
  }
  return result;
}

connection_handle abstract_broker::add_tcp_scribe(const std::string& host,
                                                  uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(host) << ", " << CAF_ARG(port));
  return backend().add_tcp_scribe(this, host, port);
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
  while (!m_doormen.empty()) {
    // stop_reading will remove the doorman from m_doormen
    m_doormen.begin()->second->stop_reading();
  }
  while (!m_scribes.empty()) {
    // stop_reading will remove the scribe from m_scribes
    m_scribes.begin()->second->stop_reading();
  }
}

void abstract_broker::close(connection_handle hdl) {
  by_id(hdl).stop_reading();
}

void abstract_broker::close(accept_handle hdl) {
  by_id(hdl).stop_reading();
}

bool abstract_broker::valid(connection_handle hdl) {
  return m_scribes.count(hdl) > 0;
}

bool abstract_broker::valid(accept_handle hdl) {
  return m_doormen.count(hdl) > 0;
}

abstract_broker::abstract_broker() : m_mm(*middleman::instance()) {
  // nop
}

abstract_broker::abstract_broker(middleman& ptr) : m_mm(ptr) {
  // nop
}

network::multiplexer& abstract_broker::backend() {
  return m_mm.backend();
}

} // namespace io
} // namespace caf
