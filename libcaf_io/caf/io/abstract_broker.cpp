// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/broker.hpp"
#include "caf/io/network/multiplexer.hpp"

#include "caf/actor_system.hpp"
#include "caf/config.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/none.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"
#include "caf/span.hpp"

namespace caf::io {

bool abstract_broker::enqueue(strong_actor_ptr src, message_id mid, message msg,
                              execution_unit*) {
  return enqueue(make_mailbox_element(std::move(src), mid, {}, std::move(msg)),
                 backend_);
}

bool abstract_broker::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  CAF_PUSH_AID(id());
  return scheduled_actor::enqueue(std::move(ptr), backend_);
}

void abstract_broker::launch(execution_unit* eu, bool lazy, bool hide) {
  CAF_PUSH_AID_FROM_PTR(this);
  CAF_ASSERT(eu != nullptr);
  CAF_ASSERT(dynamic_cast<network::multiplexer*>(eu) != nullptr);
  backend_ = static_cast<network::multiplexer*>(eu);
  CAF_LOG_TRACE(CAF_ARG(lazy) << CAF_ARG(hide));
  if (!hide)
    register_at_system();
  if (lazy && mailbox().try_block())
    return;
  intrusive_ptr_add_ref(ctrl());
  eu->exec_later(this);
}

bool abstract_broker::cleanup(error&& reason, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  close_all();
  CAF_ASSERT(doormen_.empty());
  CAF_ASSERT(scribes_.empty());
  CAF_ASSERT(datagram_servants_.empty());
  return scheduled_actor::cleanup(std::move(reason), host);
}

abstract_broker::~abstract_broker() {
  // nop
}

void abstract_broker::configure_read(connection_handle hdl,
                                     receive_policy::config cfg) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(cfg));
  if (auto x = by_id(hdl))
    x->configure_read(cfg);
}

void abstract_broker::ack_writes(connection_handle hdl, bool enable) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(enable));
  if (auto x = by_id(hdl))
    x->ack_writes(enable);
}

byte_buffer& abstract_broker::wr_buf(connection_handle hdl) {
  CAF_ASSERT(hdl != invalid_connection_handle);
  if (auto x = by_id(hdl)) {
    return x->wr_buf();
  } else {
    CAF_LOG_ERROR("tried to access wr_buf() of an unknown connection_handle:"
                  << CAF_ARG(hdl));
    return dummy_wr_buf_;
  }
}

void abstract_broker::write(connection_handle hdl, size_t bs, const void* buf) {
  auto& out = wr_buf(hdl);
  auto first = reinterpret_cast<const std::byte*>(buf);
  auto last = first + bs;
  out.insert(out.end(), first, last);
}

void abstract_broker::write(connection_handle hdl, span<const std::byte> buf) {
  write(hdl, buf.size(), buf.data());
}

void abstract_broker::flush(connection_handle hdl) {
  if (auto x = by_id(hdl))
    x->flush();
}

void abstract_broker::ack_writes(datagram_handle hdl, bool enable) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(enable));
  if (auto x = by_id(hdl))
    x->ack_writes(enable);
}

byte_buffer& abstract_broker::wr_buf(datagram_handle hdl) {
  if (auto x = by_id(hdl)) {
    return x->wr_buf(hdl);
  } else {
    CAF_LOG_ERROR("tried to access wr_buf() of an unknown"
                  "datagram_handle");
    return dummy_wr_buf_;
  }
}

void abstract_broker::enqueue_datagram(datagram_handle hdl, byte_buffer buf) {
  if (auto x = by_id(hdl))
    x->enqueue_datagram(hdl, std::move(buf));
  else
    CAF_LOG_ERROR("tried to access datagram_buffer() of an unknown"
                  "datagram_handle");
}

void abstract_broker::write(datagram_handle hdl, size_t bs, const void* buf) {
  auto& out = wr_buf(hdl);
  auto first = reinterpret_cast<const std::byte*>(buf);
  auto last = first + bs;
  out.insert(out.end(), first, last);
}

void abstract_broker::flush(datagram_handle hdl) {
  if (auto x = by_id(hdl))
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

void abstract_broker::add_scribe(scribe_ptr ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  add_servant(std::move(ptr));
}

connection_handle abstract_broker::add_scribe(network::native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  return add_servant(backend().new_scribe(fd));
}

expected<connection_handle>
abstract_broker::add_tcp_scribe(const std::string& hostname, uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(hostname) << ", " << CAF_ARG(port));
  auto eptr = backend().new_tcp_scribe(hostname, port);
  if (eptr)
    return add_servant(std::move(*eptr));
  return std::move(eptr.error());
}
void abstract_broker::move_scribe(scribe_ptr ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  move_servant(std::move(ptr));
}

void abstract_broker::add_doorman(doorman_ptr ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  add_servant(std::move(ptr));
}

accept_handle abstract_broker::add_doorman(network::native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  return add_servant(backend().new_doorman(fd));
}

expected<std::pair<accept_handle, uint16_t>>
abstract_broker::add_tcp_doorman(uint16_t port, const char* in,
                                 bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(in) << CAF_ARG(reuse_addr));
  auto eptr = backend().new_tcp_doorman(port, in, reuse_addr);
  if (eptr) {
    auto ptr = std::move(*eptr);
    auto p = ptr->port();
    return std::make_pair(add_servant(std::move(ptr)), p);
  }
  return std::move(eptr.error());
}

void abstract_broker::add_datagram_servant(datagram_servant_ptr ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(ptr->parent() == nullptr);
  ptr->set_parent(this);
  auto hdls = ptr->hdls();
  launch_servant(ptr);
  for (auto& hdl : hdls)
    add_hdl_for_datagram_servant(ptr, hdl);
  auto hdl = ptr->hdl();
  add_hdl_for_datagram_servant(std::move(ptr), hdl);
}

void abstract_broker::add_hdl_for_datagram_servant(datagram_servant_ptr ptr,
                                                   datagram_handle hdl) {
  CAF_LOG_TRACE(CAF_ARG(ptr) << CAF_ARG(hdl));
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(ptr->parent() == this);
  get_map(hdl).emplace(hdl, std::move(ptr));
}

datagram_handle
abstract_broker::add_datagram_servant(network::native_socket fd) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  auto ptr = backend().new_datagram_servant(fd);
  auto hdl = ptr->hdl();
  add_datagram_servant(std::move(ptr));
  return hdl;
}

datagram_handle abstract_broker::add_datagram_servant_for_endpoint(
  network::native_socket fd, const network::ip_endpoint& ep) {
  CAF_LOG_TRACE(CAF_ARG(fd));
  auto ptr = backend().new_datagram_servant_for_endpoint(fd, ep);
  auto hdl = ptr->hdl();
  add_datagram_servant(std::move(ptr));
  return hdl;
}

expected<datagram_handle>
abstract_broker::add_udp_datagram_servant(const std::string& host,
                                          uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(host) << CAF_ARG(port));
  auto eptr = backend().new_remote_udp_endpoint(host, port);
  if (eptr) {
    auto ptr = std::move(*eptr);
    auto hdl = ptr->hdl();
    add_datagram_servant(std::move(ptr));
    return hdl;
  }
  return std::move(eptr.error());
}

expected<std::pair<datagram_handle, uint16_t>>
abstract_broker::add_udp_datagram_servant(uint16_t port, const char* in,
                                          bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(in) << CAF_ARG(reuse_addr));
  auto eptr = backend().new_local_udp_endpoint(port, in, reuse_addr);
  if (eptr) {
    auto ptr = std::move(*eptr);
    auto p = ptr->local_port();
    auto hdl = ptr->hdl();
    add_datagram_servant(std::move(ptr));
    return std::make_pair(hdl, p);
  }
  return std::move(eptr.error());
}

void abstract_broker::move_datagram_servant(datagram_servant_ptr ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(ptr->parent() != nullptr && ptr->parent() != this);
  ptr->set_parent(this);
  CAF_ASSERT(ptr->parent() == this);
  auto hdls = ptr->hdls();
  for (auto& hdl : hdls)
    add_hdl_for_datagram_servant(ptr, hdl);
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

datagram_handle abstract_broker::datagram_hdl_by_port(uint16_t port) {
  for (auto& kvp : datagram_servants_)
    if (kvp.second->port(kvp.first) == port)
      return kvp.first;
  return invalid_datagram_handle;
}

std::string abstract_broker::remote_addr(datagram_handle hdl) {
  auto i = datagram_servants_.find(hdl);
  return i != datagram_servants_.end() ? i->second->addr(hdl) : std::string{};
}

uint16_t abstract_broker::remote_port(datagram_handle hdl) {
  auto i = datagram_servants_.find(hdl);
  return i != datagram_servants_.end() ? i->second->port(hdl) : 0;
}

uint16_t abstract_broker::local_port(datagram_handle hdl) {
  auto i = datagram_servants_.find(hdl);
  return i != datagram_servants_.end() ? i->second->local_port() : 0;
}

bool abstract_broker::remove_endpoint(datagram_handle hdl) {
  if (auto x = by_id(hdl)) {
    x->remove_endpoint(hdl);
    return true;
  } else {
    return false;
  }
}

void abstract_broker::close_all() {
  CAF_LOG_TRACE("");
  // Calling graceful_shutdown causes the objects to detach from the broker by
  // removing from the container.
  while (!doormen_.empty())
    doormen_.begin()->second->graceful_shutdown();
  while (!scribes_.empty())
    scribes_.begin()->second->graceful_shutdown();
  while (!datagram_servants_.empty())
    datagram_servants_.begin()->second->graceful_shutdown();
}

resumable::subtype_t abstract_broker::subtype() const {
  return io_actor;
}

resumable::resume_result abstract_broker::resume(execution_unit* ctx,
                                                 size_t mt) {
  CAF_ASSERT(ctx != nullptr);
  CAF_ASSERT(ctx == backend_);
  return scheduled_actor::resume(ctx, mt);
}

const char* abstract_broker::name() const {
  return "user.broker";
}

void abstract_broker::init_broker() {
  CAF_LOG_TRACE("");
  setf(is_initialized_flag);
  // launch backends now, because user-defined initialization
  // might call functions like add_connection
  for (auto& kvp : doormen_)
    kvp.second->launch();
}

abstract_broker::abstract_broker(actor_config& cfg) : scheduled_actor(cfg) {
  // nop
}

void abstract_broker::launch_servant(doorman_ptr& ptr) {
  // A doorman needs to be launched in addition to being initialized. This
  // allows CAF to assign doorman to uninitialized brokers.
  if (getf(is_initialized_flag))
    ptr->launch();
}

void abstract_broker::launch_servant(datagram_servant_ptr& ptr) {
  if (getf(is_initialized_flag))
    ptr->launch();
}

} // namespace caf::io
