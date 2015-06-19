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
#include "caf/exception.hpp"
#include "caf/to_string.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/make_counted.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/system_messages.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/ripemd_160.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/singletons.hpp"
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
class uti_impl : public abstract_uniform_type_info<T> {
public:
  using super = abstract_uniform_type_info<T>;

  explicit uti_impl(const char* tname) : super(tname) {
    // nop
  }

  void serialize(const void* instance, serializer* sink) const {
    serialize_impl(super::deref(instance), sink);
  }

  void deserialize(void* instance, deserializer* source) const {
    deserialize_impl(super::deref(instance), source);
  }
};

template <class T>
void do_announce(const char* tname) {
  announce(typeid(T), uniform_type_info_ptr{new uti_impl<T>(tname)});
}

} // namespace <anonymous>

using middleman_actor_base = middleman_actor::extend<
                               reacts_to<ok_atom, int64_t>,
                               reacts_to<ok_atom, int64_t, actor_addr>,
                               reacts_to<error_atom, int64_t, std::string>
                             >;

class middleman_actor_impl : public middleman_actor_base::base {
public:
  middleman_actor_impl(middleman& mref, actor default_broker)
      : broker_(default_broker),
        parent_(mref),
        next_request_id_(0) {
    // nop
  }

  ~middleman_actor_impl();

  void on_exit() {
    CAF_LOG_TRACE("");
    pending_gets_.clear();
    pending_deletes_.clear();
    broker_ = invalid_actor;
  }

  using get_op_result = either<ok_atom, actor_addr>
                        ::or_else<error_atom, std::string>;

  using get_op_promise = typed_response_promise<get_op_result>;

  using del_op_result = either<ok_atom>::or_else<error_atom, std::string>;

  using del_op_promise = typed_response_promise<del_op_result>;

  using map_type = std::map<int64_t, response_promise>;

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
      [=](get_atom, const std::string& hostname, uint16_t port,
          std::set<std::string>& expected_ifs) {
        return get(hostname, port, std::move(expected_ifs));
      },
      [=](get_atom, const std::string& hostname, uint16_t port) {
        return get(hostname, port, std::set<std::string>());
      },
      [=](delete_atom, const actor_addr& whom) {
        return del(whom);
      },
      [=](delete_atom, const actor_addr& whom, uint16_t port) {
        return del(whom, port);
      },
      [=](ok_atom, int64_t request_id) {
        // not legal for get results
        CAF_ASSERT(pending_gets_.count(request_id) == 0);
        handle_ok<del_op_result>(pending_deletes_, request_id);
      },
      [=](ok_atom, int64_t request_id, actor_addr& result) {
        // not legal for delete results
        CAF_ASSERT(pending_deletes_.count(request_id) == 0);
        handle_ok<get_op_result>(pending_gets_, request_id, std::move(result));
      },
      [=](error_atom, int64_t request_id, std::string& reason) {
        handle_error(request_id, reason);
      }
    };
  }

private:
  either<ok_atom, uint16_t>::or_else<error_atom, std::string>
  put(const actor_addr& whom, uint16_t port,
      const char* in = nullptr, bool reuse_addr = false) {
    CAF_LOG_TRACE(CAF_TSARG(whom) << ", " << CAF_ARG(port)
                  << ", " << CAF_ARG(reuse_addr));
    accept_handle hdl;
    uint16_t actual_port;
    try {
      // treat empty strings like nullptr
      if (in != nullptr && in[0] == '\0') {
        in = nullptr;
      }
      auto res = parent_.backend().new_tcp_doorman(port, in, reuse_addr);
      hdl = res.first;
      actual_port = res.second;
    }
    catch (bind_failure& err) {
      return {error_atom::value, std::string("bind_failure: ") + err.what()};
    }
    catch (network_error& err) {
      return {error_atom::value, std::string("network_error: ") + err.what()};
    }
    send(broker_, put_atom::value, hdl, whom, actual_port);
    return {ok_atom::value, actual_port};
  }

  get_op_promise get(const std::string& hostname, uint16_t port,
                     std::set<std::string> expected_ifs) {
    CAF_LOG_TRACE(CAF_ARG(hostname) << ", " << CAF_ARG(port));
    auto result = make_response_promise();
    try {
      auto hdl = parent_.backend().new_tcp_scribe(hostname, port);
      auto req_id = next_request_id_++;
      send(broker_, get_atom::value, hdl, req_id,
           actor{this}, std::move(expected_ifs));
      pending_gets_.emplace(req_id, result);
    }
    catch (network_error& err) {
      // fullfil promise immediately
      std::string msg = "network_error: ";
      msg += err.what();
      result.deliver(get_op_result{error_atom::value, std::move(msg)}.value);
    }
    return result;
  }

  del_op_promise del(const actor_addr& whom, uint16_t port = 0) {
    CAF_LOG_TRACE(CAF_TSARG(whom) << ", " << CAF_ARG(port));
    auto result = make_response_promise();
    auto req_id = next_request_id_++;
    send(broker_, delete_atom::value, req_id, whom, port);
    pending_deletes_.emplace(req_id, result);
    return result;
  }

  template <class T, class... Ts>
  void handle_ok(map_type& storage, int64_t request_id, Ts&&... xs) {
    CAF_LOG_TRACE(CAF_ARG(request_id));
    auto i = storage.find(request_id);
    if (i == storage.end()) {
      CAF_LOG_ERROR("request id not found: " << request_id);
      return;
    }
    i->second.deliver(T{ok_atom::value, std::forward<Ts>(xs)...}.value);
    storage.erase(i);
  }

  template <class F>
  bool finalize_request(map_type& storage, int64_t req_id, F fun) {
    CAF_LOG_TRACE(CAF_ARG(req_id));
    auto i = storage.find(req_id);
    if (i == storage.end()) {
      CAF_LOG_INFO("request ID not found in storage");
      return false;
    }
    fun(i->second);
    storage.erase(i);
    return true;
  }

  void handle_error(int64_t request_id, std::string& reason) {
    CAF_LOG_TRACE(CAF_ARG(request_id) << ", " << CAF_ARG(reason));
    auto fget = [&](response_promise& rp) {
      rp.deliver(get_op_result{error_atom::value, std::move(reason)}.value);
    };
    auto fdel = [&](response_promise& rp) {
      rp.deliver(del_op_result{error_atom::value, std::move(reason)}.value);
    };
    if (! finalize_request(pending_gets_, request_id, fget)
        && ! finalize_request(pending_deletes_, request_id, fdel)) {
      CAF_LOG_ERROR("invalid request id: " << request_id);
    }
  }

  actor broker_;
  middleman& parent_;
  int64_t next_request_id_;
  map_type pending_gets_;
  map_type pending_deletes_;
};

middleman_actor_impl::~middleman_actor_impl() {
  CAF_LOG_TRACE("");
}

middleman* middleman::instance() {
  CAF_LOGF_TRACE("");
  // store lambda in a plain old function pointer to make sure
  // std::function has minimal overhead
  using funptr = backend_pointer (*)();
  funptr backend_fac = [] { return network::multiplexer::make(); };
  auto fac = [&] { return new middleman(backend_fac); };
  auto sid = detail::singletons::middleman_plugin_id;
  auto res = detail::singletons::get_plugin_singleton(sid, fac);
  return static_cast<middleman*>(res);
}

void middleman::add_broker(broker_ptr bptr) {
  brokers_.insert(bptr);
  bptr->attach_functor([=](uint32_t) { brokers_.erase(bptr); });
}

void middleman::initialize() {
  CAF_LOG_TRACE("");
  backend_supervisor_ = backend_->make_supervisor();
  thread_ = std::thread{[this] {
    CAF_LOG_TRACE("");
    backend_->run();
  }};
  backend_->thread_id(thread_.get_id());
  // announce io-related types
  announce<network::protocol>("caf::io::network::protocol");
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
  manager_ = spawn_typed<middleman_actor_impl, detached + hidden>(*this, mgr);
}

void middleman::stop() {
  CAF_LOG_TRACE("");
  backend_->dispatch([=] {
    CAF_LOG_TRACE("");
    notify<hook::before_shutdown>();
    // managers_ will be modified while we are stopping each manager,
    // because each manager will call remove(...)
    for (auto& kvp : named_brokers_) {
      if (kvp.second->exit_reason() == exit_reason::not_exited) {
        kvp.second->cleanup(exit_reason::normal);
      }
    }
  });
  backend_supervisor_.reset();
  thread_.join();
  hooks_.reset();
  named_brokers_.clear();
  scoped_actor self(true);
  self->monitor(manager_);
  self->send_exit(manager_, exit_reason::user_shutdown);
  self->receive(
    [](const down_msg&) {
      // nop
    }
  );
}

void middleman::dispose() {
  delete this;
}

middleman::middleman(const backend_factory& factory) : backend_(factory()) {
  // nop
}

middleman::~middleman() {
  // nop
}

middleman_actor middleman::actor_handle() {
  return manager_;
}

middleman_actor get_middleman_actor() {
  return middleman::instance()->actor_handle();
}

} // namespace io
} // namespace caf
