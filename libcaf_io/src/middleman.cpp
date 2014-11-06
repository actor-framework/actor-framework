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
#include "caf/uniform_type_info.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/remote_actor_proxy.hpp"

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
  uti_impl() : m_native(&typeid(T)), m_name(detail::demangle<T>()) {
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

template <class... Ts>
struct announce_helper;

template <class T, class... Ts>
struct announce_helper<T, Ts...> {
  static inline void exec() {
    announce(typeid(T), uniform_type_info_ptr{new uti_impl<T>});
    announce_helper<Ts...>::exec();
  }
};

template <>
struct announce_helper<> {
  static inline void exec() {
    // end of recursion
  }
};

} // namespace <anonymous>

using detail::make_counted;

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
  m_supervisor = m_backend->make_supervisor();
  m_thread = std::thread([this] {
    CAF_LOGC_TRACE("caf::io::middleman", "initialize$run", "");
    m_backend->run();
  });
  m_backend->thread_id(m_thread.get_id());
  // announce io-related types
  announce_helper<new_data_msg, new_connection_msg,
                  acceptor_closed_msg, connection_closed_msg,
                  accept_handle, acceptor_closed_msg,
                  connection_closed_msg, connection_handle,
                  new_connection_msg, new_data_msg>::exec();
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
  m_supervisor.reset();
  m_thread.join();
  m_named_brokers.clear();
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

} // namespace io
} // namespace caf
