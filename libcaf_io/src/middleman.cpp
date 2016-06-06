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

#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/actor.hpp"
#include "caf/after.hpp"
#include "caf/config.hpp"
#include "caf/logger.hpp"
#include "caf/node_id.hpp"
#include "caf/exception.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/make_counted.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/typed_event_based_actor.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/system_messages.hpp"

#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/detail/ripemd_160.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/get_root_uuid.hpp"
#include "caf/actor_registry.hpp"
#include "caf/detail/get_mac_addresses.hpp"

#ifdef CAF_USE_ASIO
#include "caf/io/network/asio_multiplexer.hpp"
#include "caf/io/network/asio_multiplexer_impl.hpp"
#endif // CAF_USE_ASIO

#ifdef CAF_WINDOWS
#include <io.h>
#include <fcntl.h>
#endif // CAF_WINDOWS

namespace caf {
namespace io {

actor_system::module* middleman::make(actor_system& sys, detail::type_list<>) {
  class impl : public middleman {
  public:
    impl(actor_system& ref) : middleman(ref), backend_(&ref) {
      // nop
    }

    network::multiplexer& backend() override {
      return backend_;
    }

  private:
    network::default_multiplexer backend_;
  };
# ifdef CAF_USE_ASIO
  class asio_impl : public middleman {
  public:
    asio_impl(actor_system& ref) : middleman(ref), backend_(&ref) {
      // nop
    }

    network::multiplexer& backend() override {
      return backend_;
    }

  private:
    network::asio_multiplexer backend_;
  };
  if (sys.backend_name() == atom("asio"))
    return new asio_impl(sys);
# endif // CAF_USE_ASIO
  return new impl(sys);
}

middleman::middleman(actor_system& sys)
    : system_(sys),
      manager_(unsafe_actor_handle_init),
      heartbeat_interval_(0),
      enable_automatic_connections_(false) {
  // nop
}

uint16_t middleman::publish(const strong_actor_ptr& whom,
                            std::set<std::string> sigs,
                            uint16_t port, const char* in, bool ru) {
  CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(sigs) << CAF_ARG(port)
                << CAF_ARG(in) << CAF_ARG(ru));
  if (! whom)
    throw network_error("cannot publish an invalid actor");
  std::string str;
  if (in != nullptr)
    str = in;
  auto mm = actor_handle();
  scoped_actor self{system()};
  uint16_t result;
  std::string error_msg;
  try {
    self->request(mm, infinite, publish_atom::value, port,
                  std::move(whom), std::move(sigs), str, ru).receive(
      [&](ok_atom, uint16_t res) {
        result = res;
      },
      [&](const error& err) {
        error_msg = system().render(err);
      }
    );
  }
  catch (actor_exited& e) {
    error_msg = "scoped actor in caf::publish quit unexpectedly: ";
    error_msg += e.what();
  }
  if (! error_msg.empty())
    throw network_error(std::move(error_msg));
  return result;
}

uint16_t middleman::publish_local_groups(uint16_t port, const char* in) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(in));
  auto group_nameserver = [](event_based_actor* self) -> behavior {
    return {
      [self](get_atom, const std::string& name) {
        return self->system().groups().get("local", name);
      }
    };
  };
  auto gn = system().spawn<hidden>(group_nameserver);
  try {
    auto result = publish(gn, port, in);
    add_shutdown_cb([gn] {
      anon_send_exit(gn, exit_reason::user_shutdown);
    });
    return result;
  }
  catch (std::exception&) {
    anon_send_exit(gn, exit_reason::user_shutdown);
    throw;
  }
}

void middleman::unpublish(const actor_addr& whom, uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(port));
  scoped_actor self{system(), true};
  self->request(actor_handle(), infinite,
                unpublish_atom::value, whom, port).receive(
    [] {
      // ok, basp_broker is done
    },
    [](const error&) {
      // ok, ignore errors
    }
  );
}

strong_actor_ptr middleman::remote_actor(std::set<std::string> ifs,
                                         std::string host, uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(ifs) << CAF_ARG(host) << CAF_ARG(port));
  auto mm = actor_handle();
  strong_actor_ptr result;
  scoped_actor self{system(), true};
  self->request(mm, infinite, connect_atom::value,
                std::move(host), port).receive(
    [&](ok_atom, const node_id&, strong_actor_ptr res, std::set<std::string>& xs) {
      CAF_LOG_TRACE(CAF_ARG(res) << CAF_ARG(xs));
      if (!res)
        throw network_error("no actor published at port "
                            + std::to_string(port));
      if (! (xs.empty() && ifs.empty())
          && ! std::includes(xs.begin(), xs.end(), ifs.begin(), ifs.end())) {
        std::string what = "expected signature: ";
        what += deep_to_string(ifs);
        what += ", found: ";
        what += deep_to_string(xs);

        throw network_error(std::move(what));
      }
      result.swap(res);
    },
    [&](const error& msg) {
      CAF_LOG_TRACE(CAF_ARG(msg));
      throw network_error(system().render(msg));
    }
  );
  return result;
}

group middleman::remote_group(const std::string& group_uri) {
  CAF_LOG_TRACE(CAF_ARG(group_uri));
  // format of group_identifier is group@host:port
  // a regex would be the natural choice here, but we want to support
  // older compilers that don't have <regex> implemented (e.g. GCC < 4.9)
  auto pos1 = group_uri.find('@');
  auto pos2 = group_uri.find(':');
  auto last = std::string::npos;
  if (pos1 == last || pos2 == last || pos1 >= pos2) {
    throw std::invalid_argument("group_uri has an invalid format");
  }
  auto name = group_uri.substr(0, pos1);
  auto host = group_uri.substr(pos1 + 1, pos2 - pos1 - 1);
  auto port = static_cast<uint16_t>(std::stoi(group_uri.substr(pos2 + 1)));
  return remote_group(name, host, port);
}

group middleman::remote_group(const std::string& group_identifier,
                              const std::string& host, uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(group_identifier) << CAF_ARG(host) << CAF_ARG(port));
  auto group_server = remote_actor(host, port);
  scoped_actor self{system(), true};
  self->send(group_server, get_atom::value, group_identifier);
  group result;
  self->receive(
    [&](group& grp) {
      result = std::move(grp);
    }
  );
  return result;
}

/*
actor middleman::remote_lookup(atom_value name, const node_id& nid) {
  CAF_LOG_TRACE(CAF_ARG(name) << CAF_ARG(nid));
  auto basp = named_broker<basp_broker>(atom("BASP"));
  actor result;
  scoped_actor self{system(), true};
  try {
    self->send(basp, forward_atom::value, self.address(), nid, name,
               make_message(sys_atom::value, get_atom::value, "info"));
    self->receive(
      [&](ok_atom, const std::string&,
          const actor_addr& addr, const std::string&) {
        result = actor_cast<actor>(addr);
      },
      after(std::chrono::minutes(5)) >> [] {
        // nop
      }
    );
  } catch (...) {
    // nop
  }
  return result;
}
*/

void middleman::start() {
  CAF_LOG_TRACE("");
  backend_supervisor_ = backend().make_supervisor();
  if (! backend_supervisor_) {
    // the only backend that returns a `nullptr` is the `test_multiplexer`
    // which does not have its own thread but uses the main thread instead
    backend().thread_id(std::this_thread::get_id());
  } else {
    thread_ = std::thread{[this] {
      CAF_SET_LOGGER_SYS(&system());
      CAF_LOG_TRACE("");
      backend().run();
    }};
    backend().thread_id(thread_.get_id());
  }
  auto basp = named_broker<basp_broker>(atom("BASP"));
  manager_ = make_middleman_actor(system(), basp);
}

void middleman::stop() {
  CAF_LOG_TRACE("");
  backend().dispatch([=] {
    CAF_LOG_TRACE("");
    notify<hook::before_shutdown>();
    // managers_ will be modified while we are stopping each manager,
    // because each manager will call remove(...)
    for (auto& kvp : named_brokers_) {
      auto& hdl = kvp.second;
      auto ptr = static_cast<broker*>(actor_cast<abstract_actor*>(hdl));
      if (! ptr->is_terminated()) {
        ptr->context(&backend());
        ptr->is_terminated(true);
        ptr->finished();
      }
    }
  });
  backend_supervisor_.reset();
  if (thread_.joinable())
    thread_.join();
  hooks_.reset();
  named_brokers_.clear();
  scoped_actor self{system(), true};
  self->send_exit(manager_, exit_reason::user_shutdown);
  self->wait_for(manager_);
  invalidate(manager_);
}

void middleman::init(actor_system_config& cfg) {
  // logging not available at this stage
  // add I/O-related types to config
  cfg.add_message_type<network::protocol>("@protocol")
     .add_message_type<network::address_listing>("@address_listing")
     .add_message_type<new_data_msg>("@new_data_msg")
     .add_message_type<new_connection_msg>("@new_connection_msg")
     .add_message_type<acceptor_closed_msg>("@acceptor_closed_msg")
     .add_message_type<connection_closed_msg>("@connection_closed_msg")
     .add_message_type<accept_handle>("@accept_handle")
     .add_message_type<acceptor_closed_msg>("@acceptor_closed_msg")
     .add_message_type<connection_closed_msg>("@connection_closed_msg")
     .add_message_type<connection_handle>("@connection_handle")
     .add_message_type<new_connection_msg>("@new_connection_msg")
     .add_message_type<new_data_msg>("@new_data_msg");
  // compute and set ID for this network node
  node_id this_node{node_id::data::create_singleton()};
  cfg.network_id.swap(this_node);
  // set scheduling parameters for multiplexer
  backend().max_throughput(cfg.scheduler_max_throughput);
  backend().max_consecutive_reads(cfg.middleman_max_consecutive_reads);
  // set options relevant to BASP
  heartbeat_interval_ = cfg.middleman_heartbeat_interval;
  enable_automatic_connections_ = cfg.middleman_enable_automatic_connections;
  // enable slave mode
  cfg.slave_mode_fun = &middleman::exec_slave_mode;
}

actor_system::module::id_t middleman::id() const {
  return module::middleman;
}

void* middleman::subtype_ptr() {
  return this;
}

middleman::~middleman() {
  // nop
}

middleman_actor middleman::actor_handle() {
  return manager_;
}

int middleman::exec_slave_mode(actor_system&, const actor_system_config&) {
  // TODO
  return 0;
}

} // namespace io
} // namespace caf
