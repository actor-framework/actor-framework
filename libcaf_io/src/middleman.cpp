/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/io/middleman.hpp"

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
#include "caf/actor_proxy.hpp"
#include "caf/make_counted.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/function_view.hpp"
#include "caf/actor_registry.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/raw_event_based_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

#include "caf/io/basp_broker.hpp"
#include "caf/io/system_messages.hpp"

#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/detail/ripemd_160.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/get_root_uuid.hpp"
#include "caf/detail/get_mac_addresses.hpp"
#include "caf/detail/incoming_stream_multiplexer.hpp"
#include "caf/detail/outgoing_stream_multiplexer.hpp"

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
  if (sys.config().middleman_network_backend == atom("asio"))
    return new asio_impl(sys);
# endif // CAF_USE_ASIO
  return new impl(sys);
}

middleman::middleman(actor_system& sys) : system_(sys) {
  // nop
}

expected<strong_actor_ptr> middleman::remote_spawn_impl(const node_id& nid,
                                                        std::string& name,
                                                        message& args,
                                                        std::set<std::string> s,
                                                        duration timeout) {
  auto f = make_function_view(actor_handle(), timeout);
  return f(spawn_atom::value, nid, std::move(name),
           std::move(args), std::move(s));
}

expected<uint16_t> middleman::open(uint16_t port, const char* cstr, bool ru) {
  std::string str;
  if (cstr != nullptr)
    str = cstr;
  auto f = make_function_view(actor_handle());
  return f(open_atom::value, port, std::move(str), ru);
}

expected<void> middleman::close(uint16_t port) {
  auto f = make_function_view(actor_handle());
  return f(close_atom::value, port);
}

expected<node_id> middleman::connect(std::string host, uint16_t port) {
  auto f = make_function_view(actor_handle());
  auto res = f(connect_atom::value, std::move(host), port);
  if (!res)
    return std::move(res.error());
  return std::get<0>(*res);
}

expected<uint16_t> middleman::publish(const strong_actor_ptr& whom,
                                      std::set<std::string> sigs, uint16_t port,
                                      const char* cstr, bool ru) {
  CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(sigs) << CAF_ARG(port));
  if (!whom)
    return sec::cannot_publish_invalid_actor;
  std::string in;
  if (cstr != nullptr)
    in = cstr;
  auto f = make_function_view(actor_handle());
  return f(publish_atom::value, port, std::move(whom), std::move(sigs), in, ru);
}

expected<uint16_t> middleman::publish_local_groups(uint16_t port,
                                                   const char* in) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(in));
  auto group_nameserver = [](event_based_actor* self) -> behavior {
    return {
      [self](get_atom, const std::string& name) {
        return self->system().groups().get_local(name);
      }
    };
  };
  auto gn = system().spawn<hidden>(group_nameserver);
  auto result = publish(gn, port, in);
  // link gn to our manager
  if (result)
    manager_->add_link(actor_cast<abstract_actor*>(gn));
  else
    anon_send_exit(gn, exit_reason::user_shutdown);
  return result;
}

expected<void> middleman::unpublish(const actor_addr& whom, uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(port));
  auto f = make_function_view(actor_handle());
  return f(unpublish_atom::value, whom, port);
}

expected<strong_actor_ptr> middleman::remote_actor(std::set<std::string> ifs,
                                                   std::string host,
                                                   uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(ifs) << CAF_ARG(host) << CAF_ARG(port));
  auto f = make_function_view(actor_handle());
  auto res = f(connect_atom::value, std::move(host), port);
  if (!res)
    return std::move(res.error());
  strong_actor_ptr ptr = std::move(std::get<1>(*res));
  if (!ptr)
    return make_error(sec::no_actor_published_at_port, port);
  if (!system().assignable(std::get<2>(*res), ifs))
    return make_error(sec::unexpected_actor_messaging_interface, std::move(ifs),
                      std::move(std::get<2>(*res)));
  return ptr;
}

expected<group> middleman::remote_group(const std::string& group_uri) {
  CAF_LOG_TRACE(CAF_ARG(group_uri));
  // format of group_identifier is group@host:port
  // a regex would be the natural choice here, but we want to support
  // older compilers that don't have <regex> implemented (e.g. GCC < 4.9)
  auto pos1 = group_uri.find('@');
  auto pos2 = group_uri.find(':');
  auto last = std::string::npos;
  if (pos1 == last || pos2 == last || pos1 >= pos2)
    return make_error(sec::invalid_argument, "invalid URI format", group_uri);
  auto name = group_uri.substr(0, pos1);
  auto host = group_uri.substr(pos1 + 1, pos2 - pos1 - 1);
  auto port = static_cast<uint16_t>(std::stoi(group_uri.substr(pos2 + 1)));
  return remote_group(name, host, port);
}

expected<group> middleman::remote_group(const std::string& group_identifier,
                                        const std::string& host, uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(group_identifier) << CAF_ARG(host) << CAF_ARG(port));
  auto group_server = remote_actor(host, port);
  if (!group_server)
    return std::move(group_server.error());
  scoped_actor self{system(), true};
  self->send(*group_server, get_atom::value, group_identifier);
  expected<group> result{sec::cannot_connect_to_node};
  self->receive(
    [&](group& grp) {
      result = std::move(grp);
    },
    [&](error& err) {
      result = std::move(err);
    }
  );
  return result;
}

strong_actor_ptr middleman::remote_lookup(atom_value name, const node_id& nid) {
  CAF_LOG_TRACE(CAF_ARG(name) << CAF_ARG(nid));
  if (system().node() == nid)
    return system().registry().get(name);
  auto basp = named_broker<basp_broker>(atom("BASP"));
  strong_actor_ptr result;
  scoped_actor self{system(), true};
  self->send(basp, forward_atom::value, nid, atom("ConfigServ"),
             make_message(get_atom::value, name));
  self->receive(
    [&](strong_actor_ptr& addr) {
      result = std::move(addr);
    },
    after(std::chrono::minutes(5)) >> [] {
      // nop
    }
  );
  return result;
}

void middleman::start() {
  CAF_LOG_TRACE("");
  // Create hooks.
  for (auto& f : system().config().hook_factories)
    hooks_.emplace_back(f(system_));
  // Launch backend.
  backend_supervisor_ = backend().make_supervisor();
  if (!backend_supervisor_) {
    // The only backend that returns a `nullptr` is the `test_multiplexer`
    // which does not have its own thread but uses the main thread instead.
    backend().thread_id(std::this_thread::get_id());
  } else {
    thread_ = std::thread{[this] {
      CAF_SET_LOGGER_SYS(&system());
      CAF_LOG_TRACE("");
      backend().run();
    }};
    backend().thread_id(thread_.get_id());
  }
  // Default implementation of the stream server.
  class stream_serv : public raw_event_based_actor,
                      public detail::stream_multiplexer::backend {
  public:
    stream_serv(actor_config& cfg, actor basp_ref)
        : raw_event_based_actor(cfg),
          detail::stream_multiplexer::backend(std::move(basp_ref)),
          incoming_(this, *this),
          outgoing_(this, *this) {
      // nop
    }

    const char* name() const override {
      return "stream_serv";
    }

    behavior make_behavior() override {
      return {
        [=](stream_msg& x) -> delegated<message> {
          // Dispatching depends on the direction of the message.
          if (outgoing_.has_stream(x.sid)) {
            outgoing_(x);
          } else {
            incoming_(x);
          }
          return {};
        },
        [=](sys_atom, stream_msg& x) -> delegated<message> {
          // Stream message received from a proxy, always results in a new
          // stream from a local actor to a remote node.
          CAF_ASSERT(holds_alternative<stream_msg::open>(x.content));
          outgoing_(x);
          return {};
        },
        [=](sys_atom, ok_atom, int32_t credit) {
          CAF_ASSERT(current_mailbox_element() != nullptr);
          auto cme = current_mailbox_element();
          if (cme->sender != nullptr) {
            auto& nid = cme->sender->node();
            add_credit(nid, credit);
          } else {
            CAF_LOG_ERROR("Received credit from an anonmyous stream server.");
          }
        },
        [=](exit_msg& x) {
          if (x.reason)
            quit(x.reason);
        },
        // Connects both incoming_ and outgoing_ to nid.
        [=](connect_atom, const node_id& nid) {
          send(basp_, forward_atom::value, nid, atom("ConfigServ"),
               make_message(get_atom::value, atom("StreamServ")));
        },
        // Assumes `ptr` is a remote spawn server.
        [=](strong_actor_ptr& ptr) {
          if (ptr) {
            add_remote_path(ptr->node(), ptr);
          }
        }
      };
    }

    strong_actor_ptr remote_stream_serv(const node_id& nid) override {
      strong_actor_ptr result;
      // Ask remote config server for a handle to the remote spawn server.
      scoped_actor self{system()};
      self->send(basp_, forward_atom::value, nid, atom("ConfigServ"),
                 make_message(get_atom::value, atom("StreamServ")));
      // Time out after 5 minutes.
      self->receive(
        [&](strong_actor_ptr& addr) {
          result = std::move(addr);
        },
        after(std::chrono::minutes(5)) >> [] {
          CAF_LOG_INFO("Accessing a remote spawn server timed out.");
        }
      );
      return result;
    }

    void on_exit() override {
      // Make sure to not keep references to remotes after shutdown.
      remotes().clear();
    }

  private:
    detail::incoming_stream_multiplexer incoming_;
    detail::outgoing_stream_multiplexer outgoing_;
  };
  // Spawn utility actors.
  auto basp = named_broker<basp_broker>(atom("BASP"));
  manager_ = make_middleman_actor(system(), basp);
  auto hdl = actor_cast<actor>(basp);
  auto ssi = system().spawn<stream_serv, lazy_init + hidden>(std::move(hdl));
  system().stream_serv(actor_cast<strong_actor_ptr>(std::move(ssi)));
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
      if (!ptr->getf(abstract_actor::is_terminated_flag)) {
        ptr->context(&backend());
        ptr->setf(abstract_actor::is_terminated_flag);
        ptr->finalize();
      }
    }
  });
  backend_supervisor_.reset();
  if (thread_.joinable())
    thread_.join();
  hooks_.clear();
  named_brokers_.clear();
  scoped_actor self{system(), true};
  self->send_exit(manager_, exit_reason::kill);
  if (system().config().middleman_detach_utility_actors)
    self->wait_for(manager_);
  destroy(manager_);
}

void middleman::init(actor_system_config& cfg) {
  // add remote group module to config
  struct remote_groups : group_module {
  public:
    remote_groups(middleman& parent)
        : group_module(parent.system(), "remote"),
          parent_(parent) {
      // nop
    }

    void stop() override {
      // nop
    }

    expected<group> get(const std::string& group_name) override {
      return parent_.remote_group(group_name);
    }

    error load(deserializer&, group&) override {
      // never called, because we hand out group instances of the local module
      return sec::no_such_group_module;
    }

  private:
    middleman& parent_;
  };
  auto gfactory = [=]() -> group_module* { return new remote_groups(*this); };
  cfg.group_module_factories.emplace_back(gfactory);
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
     .add_message_type<new_data_msg>("@new_data_msg")
     .add_message_type<connection_passivated_msg>("@connection_passivated_msg")
     .add_message_type<acceptor_passivated_msg>("@acceptor_passivated_msg");
  // compute and set ID for this network node
  node_id this_node{node_id::data::create_singleton()};
  system().node_.swap(this_node);
  // give config access to slave mode implementation
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
