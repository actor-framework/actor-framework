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

#include <tuple>
#include <cerrno>
#include <memory>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include "caf/on.hpp"
#include "caf/actor.hpp"
#include "caf/config.hpp"
#include "caf/node_id.hpp"
#include "caf/announce.hpp"
#include "caf/to_string.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/ripemd_160.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/make_counted.hpp"
#include "caf/detail/get_root_uuid.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/get_mac_addresses.hpp"

#ifdef CAF_WINDOWS
# include <io.h>
# include <fcntl.h>
#endif

namespace caf {
namespace io {

namespace {

template <class Subtype>
inline void serialize_impl(const handle<Subtype>& hdl, serializer* sink) {
  sink->write_value(hdl.id());
}

template <class Subtype>
inline void deserialize_impl(handle<Subtype>& hdl, deserializer* source) {
  hdl.set_id(source->read<int64_t>());
}

inline void serialize_impl(const new_connection_msg& msg, serializer* sink) {
  serialize_impl(msg.source, sink);
  serialize_impl(msg.handle, sink);
}

inline void deserialize_impl(new_connection_msg& msg, deserializer* source) {
  deserialize_impl(msg.source, source);
  deserialize_impl(msg.handle, source);
}

inline void serialize_impl(const new_data_msg& msg, serializer* sink) {
  serialize_impl(msg.handle, sink);
  auto buf_size = static_cast<uint32_t>(msg.buf.size());
  if (buf_size != msg.buf.size()) { // narrowing error
    std::ostringstream oss;
    oss << "attempted to send more than "
        << std::numeric_limits<uint32_t>::max() << " bytes";
    auto errstr = oss.str();
    CAF_LOGF_INFO(errstr);
    throw std::ios_base::failure(std::move(errstr));
  }
  sink->write_value(buf_size);
  sink->write_raw(msg.buf.size(), msg.buf.data());
}

inline void deserialize_impl(new_data_msg& msg, deserializer* source) {
  deserialize_impl(msg.handle, source);
  auto buf_size = source->read<uint32_t>();
  msg.buf.resize(buf_size);
  source->read_raw(msg.buf.size(), msg.buf.data());
}

// connection_closed_msg & acceptor_closed_msg have the same fields
template <class T>
typename std::enable_if<std::is_same<T, connection_closed_msg>::value
                        || std::is_same<T, acceptor_closed_msg>::value>::type
serialize_impl(const T& dm, serializer* sink) {
  serialize_impl(dm.handle, sink);
}

// connection_closed_msg & acceptor_closed_msg have the same fields
template <class T>
typename std::enable_if<std::is_same<T, connection_closed_msg>::value
                        || std::is_same<T, acceptor_closed_msg>::value>::type
deserialize_impl(T& dm, deserializer* source) {
  deserialize_impl(dm.handle, source);
}

template <class T>
class uti_impl : public uniform_type_info {
 public:
  uti_impl(const char* tname) : m_native(&typeid(T)), m_name(tname) {
    // nop
  }

  bool equal_to(const std::type_info& ti) const override {
    // in some cases (when dealing with dynamic libraries),
    // address can be different although types are equal
    return m_native == &ti || *m_native == ti;
  }

  bool equals(const void* lhs, const void* rhs) const override {
    return deref(lhs) == deref(rhs);
  }

  uniform_value create(const uniform_value& other) const override {
    return this->create_impl<T>(other);
  }

  message as_message(void* instance) const override {
    return make_message(deref(instance));
  }

  const char* name() const {
    return m_name.c_str();
  }

 protected:
  void serialize(const void* instance, serializer* sink) const {
    serialize_impl(deref(instance), sink);
  }

  void deserialize(void* instance, deserializer* source) const {
    deserialize_impl(deref(instance), source);
  }

 private:
  static inline T& deref(void* ptr) {
    return *reinterpret_cast<T*>(ptr);
  }

  static inline const T& deref(const void* ptr) {
    return *reinterpret_cast<const T*>(ptr);
  }

  const std::type_info* m_native;
  std::string m_name;
};

template <class T>
void do_announce(const char* tname) {
  announce(typeid(T), uniform_type_info_ptr{new uti_impl<T>(tname)});
}

} // namespace <anonymous>

using detail::make_counted;

using middleman_actor_base = middleman_actor::extend<
                               reacts_to<ok_atom, int64_t, actor_addr>,
                               reacts_to<error_atom, int64_t, std::string>
                             >::type;

class middleman_actor_impl : public middleman_actor_base::base {
 public:
  middleman_actor_impl(actor default_broker)
      : m_broker(default_broker),
        m_next_request_id(0) {
    // nop
  }

  ~middleman_actor_impl();

  using get_op_result = either<ok_atom, actor_addr>::or_else<error_atom, std::string>;

  using get_op_promise = typed_response_promise<get_op_result>;

  middleman_actor_base::behavior_type make_behavior() {
    return {
      [=](put_atom, const actor_addr& whom, uint16_t port,
          const std::string& addr, bool reuse_addr) {
        return put(whom, port, addr.c_str(), reuse_addr);
      },
      [=](put_atom, const actor_addr& whom, uint16_t port,
          const std::string& addr) {
        return put(whom, port, addr.c_str());
      },
      [=](put_atom, const actor_addr& whom, uint16_t port, bool reuse_addr) {
        return put(whom, port, nullptr, reuse_addr);
      },
      [=](put_atom, const actor_addr& whom, uint16_t port) {
        return put(whom, port);
      },
      [=](get_atom, const std::string& host, uint16_t port,
          std::set<std::string>& expected_ifs) {
        return get(host, port, std::move(expected_ifs));
      },
      [=](get_atom, const std::string& host, uint16_t port) {
        return get(host, port, std::set<std::string>());
      },
      [=](ok_atom ok, int64_t request_id, actor_addr result) {
        auto i = m_pending_requests.find(request_id);
        if (i == m_pending_requests.end()) {
          CAF_LOG_ERROR("invalid request id: " << request_id);
          return;
        }
        i->second.deliver(get_op_result{ok, result});
        m_pending_requests.erase(i);
      },
      [=](error_atom error, int64_t request_id, std::string& reason) {
        auto i = m_pending_requests.find(request_id);
        if (i == m_pending_requests.end()) {
          CAF_LOG_ERROR("invalid request id: " << request_id);
          return;
        }
        i->second.deliver(get_op_result{error, std::move(reason)});
        m_pending_requests.erase(i);
      }
    };
  }

 private:
  either<ok_atom, uint16_t>::or_else<error_atom, std::string>
  put(const actor_addr& whom, uint16_t port,
      const char* in = nullptr, bool reuse_addr = false) {
    network::native_socket fd;
    uint16_t actual_port;
    try {
      // treat empty strings like nullptr
      if (in != nullptr && in[0] == '\0') {
        in = nullptr;
      }
      auto res = network::new_ipv4_acceptor_impl(port, in, reuse_addr);
      fd = res.first;
      actual_port = res.second;
    }
    catch (bind_failure& err) {
      return {error_atom{}, std::string("bind_failure: ") + err.what()};
    }
    catch (network_error& err) {
      return {error_atom{}, std::string("network_error: ") + err.what()};
    }
    send(m_broker, put_atom{}, fd, whom, actual_port);
    return {ok_atom{}, actual_port};
  }

  get_op_promise get(const std::string& host, uint16_t port,
                     std::set<std::string> expected_ifs) {
    get_op_promise result = make_response_promise();
    try {
      auto fd = network::new_ipv4_connection_impl(host, port);
      auto req_id = m_next_request_id++;
      send(m_broker, get_atom{}, fd, req_id, actor{this}, std::move(expected_ifs));
      m_pending_requests.insert(std::make_pair(req_id, result));
    }
    catch (network_error& err) {
      // fullfil promise immediately
      std::string msg = "network_error: ";
      msg += err.what();
      result.deliver(get_op_result{error_atom{}, std::move(msg)});
    }
    return result;
  }

  actor m_broker;
  int64_t m_next_request_id;
  std::map<int64_t, get_op_promise> m_pending_requests;
};

middleman_actor_impl::~middleman_actor_impl() {
  // nop
}

middleman* middleman::instance() {
  auto sid = detail::singletons::middleman_plugin_id;
  auto fac = [] { return new middleman; };
  auto res = detail::singletons::get_plugin_singleton(sid, fac);
  return static_cast<middleman*>(res);
}

void middleman::add_broker(broker_ptr bptr) {
  m_brokers.insert(bptr);
  bptr->attach_functor([=](uint32_t) { m_brokers.erase(bptr); });
}

void middleman::initialize() {
  CAF_LOG_TRACE("");
  m_backend = network::multiplexer::make();
  m_backend_supervisor = m_backend->make_supervisor();
  m_thread = std::thread([this] {
    CAF_LOGC_TRACE("caf::io::middleman", "initialize$run", "");
    m_backend->run();
  });
  m_backend->thread_id(m_thread.get_id());
  // announce io-related types
  do_announce<new_data_msg>("caf::io::new_data_msg");
  do_announce<new_connection_msg>("caf::io::new_connection_msg");
  do_announce<acceptor_closed_msg>("caf::io::acceptor_closed_msg");
  do_announce<connection_closed_msg>("caf::io::connection_closed_msg");
  do_announce<accept_handle>("caf::io::accept_handle");
  do_announce<acceptor_closed_msg>("caf::io::acceptor_closed_msg");
  do_announce<connection_closed_msg>("caf::io::connection_closed_msg");
  do_announce<connection_handle>("caf::io::connection_handle");
  do_announce<new_connection_msg>("caf::io::new_connection_msg");
  do_announce<new_data_msg>("caf::io::new_data_msg");
  actor mgr = get_named_broker<basp_broker>(atom("_BASP"));
  m_manager = spawn_typed<middleman_actor_impl, detached + hidden>(mgr);
}

void middleman::stop() {
  CAF_LOG_TRACE("");
  m_backend->dispatch([=] {
    CAF_LOGC_TRACE("caf::io::middleman", "stop$lambda", "");
    // m_managers will be modified while we are stopping each manager,
    // because each manager will call remove(...)
    std::vector<broker_ptr> brokers;
    for (auto& kvp : m_named_brokers) {
      brokers.push_back(kvp.second);
    }
    for (auto& bro : brokers) {
      bro->close_all();
    }
  });
  m_backend_supervisor.reset();
  m_thread.join();
  m_named_brokers.clear();
  scoped_actor self(true);
  self->monitor(m_manager);
  self->send_exit(m_manager, exit_reason::user_shutdown);
  self->receive(
    [](const down_msg&) {
      // nop
    }
  );
}

void middleman::dispose() {
  delete this;
}

middleman::middleman() {
  // nop
}

middleman::~middleman() {
  // nop
}

middleman_actor middleman::actor_handle() {
  return m_manager;
}

middleman_actor get_middleman_actor() {
  return middleman::instance()->actor_handle();
}

} // namespace io
} // namespace caf
