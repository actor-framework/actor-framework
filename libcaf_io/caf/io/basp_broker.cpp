// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/basp_broker.hpp"

#include "caf/io/basp/all.hpp"
#include "caf/io/connection_helper.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/network/interfaces.hpp"

#include "caf/actor_registry.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/after.hpp"
#include "caf/anon_mail.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/forwarding_actor_proxy.hpp"
#include "caf/log/io.hpp"
#include "caf/make_counted.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"

#include <chrono>
#include <limits>

namespace {

#ifdef CAF_MSVC
#  define THREAD_LOCAL thread_local
#else
#  define THREAD_LOCAL __thread
#endif

// Used by make_proxy to detect indirect connections.
THREAD_LOCAL caf::node_id* t_last_hop = nullptr;

#undef THREAD_LOCAL

} // namespace

namespace caf::io {

// -- constructors, destructors, and assignment operators ----------------------

basp_broker::basp_broker(actor_config& cfg)
  : super(cfg),
    basp::instance::callee(super::system(),
                           static_cast<proxy_registry::backend&>(*this)),
    this_context(nullptr) {
  new (&instance) basp::instance(this, *this);
  CAF_ASSERT(this_node() != none);
}

basp_broker::~basp_broker() {
  // nop
}

// -- implementation of local_actor/broker -------------------------------------

void basp_broker::on_exit() {
  // Wait until all pending messages of workers have been shipped.
  // TODO: this blocks the calling thread. This is only safe because we know
  //       that the middleman calls this in its stop() function. However,
  //       ultimately we should find a nonblocking solution here.
  instance.hub().await_workers();
  // All nodes are offline now. We use a default-constructed error code to
  // signal ordinary shutdown.
  for (const auto& [node, observer_list] : node_observers)
    for (const auto& observer : observer_list)
      if (auto hdl = actor_cast<actor>(observer))
        anon_mail(node_down_msg{node, error{}}).send(hdl);
  node_observers.clear();
  // Release any obsolete state.
  ctx.clear();
  // Make sure all spawn servers are down before clearing the container.
  for (auto& kvp : spawn_servers)
    anon_send_exit(kvp.second, exit_reason::kill);
  // Clear remaining state.
  spawn_servers.clear();
  monitored_actors.clear();
  proxies().clear();
  instance.~instance();
}

const char* basp_broker::name() const {
  return "caf.system.basp-broker";
}

behavior basp_broker::make_behavior() {
  auto lg = log::io::trace("system.node = {}", system().node());
  set_down_handler([](local_actor* ptr, down_msg& x) {
    static_cast<basp_broker*>(ptr)->handle_down_msg(x);
  });
  if (get_or(config(), "caf.middleman.enable-automatic-connections", false)) {
    log::io::debug("enable automatic connections");
    // open a random port and store a record for our peers how to
    // connect to this broker directly in the configuration server
    auto res = add_tcp_doorman(uint16_t{0});
    if (res) {
      auto port = res->second;
      auto addrs = network::interfaces::list_addresses(false);
      auto config_server = system().registry().get("ConfigServ");
      mail(put_atom_v, "basp.default-connectivity-tcp",
           make_message(port, std::move(addrs)))
        .send(actor_cast<actor>(config_server));
    }
    automatic_connections = true;
  }
  auto heartbeat_interval = get_or(config(), "caf.middleman.heartbeat-interval",
                                   defaults::middleman::heartbeat_interval);
  if (heartbeat_interval.count() > 0) {
    auto now = clock().now();
    auto first_tick = now + heartbeat_interval;
    auto connection_timeout = get_or(config(),
                                     "caf.middleman.connection-timeout",
                                     defaults::middleman::connection_timeout);
    log::io::debug(
      "enable heartbeat heartbeat-interval = {} connection-timeout = {}",
      heartbeat_interval, connection_timeout);
    // Note: we send the scheduled time as integer representation to avoid
    //       having to assign a type ID to the time_point type.
    mail(tick_atom_v, first_tick.time_since_epoch().count(), heartbeat_interval,
         connection_timeout)
      .schedule(first_tick)
      .send(this);
  }
  return behavior{
    // received from underlying broker implementation
    [this](new_data_msg& msg) {
      auto lg = log::io::trace("msg.handle = {}", msg.handle);
      set_context(msg.handle);
      auto& ctx = *this_context;
      auto next = instance.handle(context(), msg, ctx.hdr,
                                  ctx.cstate == basp::await_payload);
      if (requires_shutdown(next)) {
        connection_cleanup(msg.handle, to_sec(next));
        close(msg.handle);
        return;
      }
      if (next != ctx.cstate) {
        auto rd_size = next == basp::await_payload ? ctx.hdr.payload_len
                                                   : basp::header_size;
        configure_read(msg.handle, receive_policy::exactly(rd_size));
        ctx.cstate = next;
      }
    },
    // received from proxy instances
    [this](forward_atom, strong_actor_ptr& src, strong_actor_ptr& dest,
           message_id mid, const message& msg) {
      auto lg = log::io::trace("src = {}, dest = {}, mid = {}, msg = {}", src,
                               dest, mid, msg);
      if (!dest || system().node() == dest->node()) {
        log::io::warning("cannot forward to invalid "
                         "or local actor: dest = {}",
                         dest);
        return;
      }
      if (src && system().node() == src->node())
        system().registry().put(src->id(), src);
      if (!instance.dispatch(context(), src, dest->node(), dest->id(), 0, mid,
                             msg)
          && mid.is_request()) {
        detail::sync_request_bouncer srb{exit_reason::remote_link_unreachable};
        srb(src, mid);
      }
    },
    // received from some system calls like whereis
    [this](forward_atom, const node_id& dest_node, uint64_t dest_id,
           const message& msg) -> result<message> {
      auto cme = current_mailbox_element();
      if (cme == nullptr || cme->sender == nullptr)
        return sec::invalid_argument;
      auto lg
        = log::io::trace("sender = {}, dest_node = {}, dest_id = {}, msg = {}",
                         cme->sender, dest_node, dest_id, msg);
      auto& sender = cme->sender;
      if (system().node() == sender->node())
        system().registry().put(sender->id(), sender);
      if (!instance.dispatch(context(), sender, dest_node, dest_id,
                             basp::header::named_receiver_flag, cme->mid,
                             msg)) {
        detail::sync_request_bouncer srb{exit_reason::remote_link_unreachable};
        srb(sender, cme->mid);
      }
      return delegated<message>();
    },
    // received from proxy instances to signal that we need to send a BASP
    // monitor_message to the origin node
    [this](monitor_atom, const strong_actor_ptr& proxy) {
      if (proxy == nullptr) {
        log::io::warning("received a monitor message from an invalid proxy");
        return;
      }
      auto route = instance.tbl().lookup(proxy->node());
      if (!route) {
        log::io::debug("connection to origin already lost, kill proxy");
        instance.proxies().erase(proxy->node(), proxy->id());
        return;
      }
      log::io::debug("write monitor_message: proxy = {}", proxy);
      // tell remote side we are monitoring this actor now
      auto hdl = route->hdl;
      instance.write_monitor_message(context(), get_buffer(hdl), proxy->node(),
                                     proxy->id());
      flush(hdl);
    },
    // received from the middleman whenever a node becomes observed by a local
    // actor
    [this](monitor_atom, const node_id& node, const actor_addr& observer) {
      // Sanity checks.
      if (!observer || !node)
        return;
      // Add to the list if a list for this node already exists.
      auto i = node_observers.find(node);
      if (i != node_observers.end()) {
        i->second.emplace_back(observer);
        return;
      }
      // Check whether the node is still connected at the moment and send the
      // observer a message immediately otherwise.
      if (!instance.tbl().lookup(node)) {
        if (auto hdl = actor_cast<actor>(observer)) {
          // TODO: we might want to consider keeping the exit reason for nodes,
          //       at least for some time. Otherwise, we'll have to send a
          //       generic "don't know" exit reason. Probably an improvement we
          //       should consider in caf_net.
          anon_mail(node_down_msg{node, sec::no_context}).send(hdl);
        }
        return;
      }
      std::vector<actor_addr> xs{observer};
      node_observers.emplace(node, std::move(xs));
    },
    // received from the middleman whenever a node becomes observed by a local
    // actor
    [this](demonitor_atom, const node_id& node, const actor_addr& observer) {
      auto i = node_observers.find(node);
      if (i == node_observers.end())
        return;
      auto& observers = i->second;
      auto j = std::find(observers.begin(), observers.end(), observer);
      if (j != observers.end()) {
        observers.erase(j);
        if (observers.empty())
          node_observers.erase(i);
      }
    },
    // received from underlying broker implementation
    [this](const new_connection_msg& msg) {
      auto lg = log::io::trace("msg.handle = {}", msg.handle);
      auto& bi = instance;
      bi.write_server_handshake(context(), get_buffer(msg.handle),
                                local_port(msg.source));
      flush(msg.handle);
      configure_read(msg.handle, receive_policy::exactly(basp::header_size));
    },
    // received from underlying broker implementation
    [this](const connection_closed_msg& msg) {
      auto lg = log::io::trace("msg.handle = {}", msg.handle);
      // We might still have pending messages from this connection. To
      // make sure there's no BASP worker deserializing a message, we are
      // sending us a message through the queue. This message gets
      // delivered only after all received messages up to this point were
      // deserialized and delivered.
      auto& q = instance.queue();
      auto msg_id = q.new_id();
      q.push(context(), msg_id, ctrl(),
             make_mailbox_element(nullptr, make_message_id(), delete_atom_v,
                                  msg.handle));
    },
    // received from the message handler above for connection_closed_msg
    [this](delete_atom, connection_handle hdl) {
      connection_cleanup(hdl, sec::none);
    },
    // received from underlying broker implementation
    [this](const acceptor_closed_msg& msg) {
      auto lg = log::io::trace("");
      // Same reasoning as in connection_closed_msg.
      auto& q = instance.queue();
      auto msg_id = q.new_id();
      q.push(context(), msg_id, ctrl(),
             make_mailbox_element(nullptr, make_message_id(), delete_atom_v,
                                  msg.handle));
    },
    // received from the message handler above for acceptor_closed_msg
    [this](delete_atom, accept_handle hdl) {
      auto port = local_port(hdl);
      instance.remove_published_actor(port);
    },
    // received from middleman actor
    [this](publish_atom, doorman_ptr& ptr, uint16_t port,
           const strong_actor_ptr& whom, std::set<std::string>& sigs) {
      auto lg = log::io::trace("ptr = {}, port = {}, whom = {}, sigs = {}", ptr,
                               port, whom, sigs);
      CAF_ASSERT(ptr != nullptr);
      add_doorman(std::move(ptr));
      if (whom)
        system().registry().put(whom->id(), whom);
      instance.add_published_actor(port, whom, std::move(sigs));
    },
    // received from test code to set up two instances without doorman
    [this](publish_atom, scribe_ptr& ptr, uint16_t port,
           const strong_actor_ptr& whom, std::set<std::string>& sigs) {
      auto lg = log::io::trace("ptr = {}, port = {}, whom = {}, sigs = {}", ptr,
                               port, whom, sigs);
      CAF_ASSERT(ptr != nullptr);
      auto hdl = ptr->hdl();
      add_scribe(std::move(ptr));
      if (whom)
        system().registry().put(whom->id(), whom);
      instance.add_published_actor(port, whom, std::move(sigs));
      set_context(hdl);
      instance.write_server_handshake(context(), get_buffer(hdl), port);
      flush(hdl);
      configure_read(hdl, receive_policy::exactly(basp::header_size));
    },
    // received from middleman actor (delegated)
    [this](connect_atom, scribe_ptr& ptr, uint16_t port) {
      auto lg = log::io::trace("ptr = {}, port = {}", ptr, port);
      CAF_ASSERT(ptr != nullptr);
      auto rp = make_response_promise();
      auto hdl = ptr->hdl();
      add_scribe(std::move(ptr));
      auto& ctx = this->ctx[hdl];
      ctx.hdl = hdl;
      ctx.remote_port = port;
      ctx.cstate = basp::await_header;
      ctx.callback = rp;
      // await server handshake
      configure_read(hdl, receive_policy::exactly(basp::header_size));
      // send client handshake
      instance.write_client_handshake(context(), get_buffer(hdl));
      flush(hdl);
    },
    [this](delete_atom, const node_id& nid, actor_id aid) {
      auto lg = log::io::trace("nid = {}, aid = {}", nid, aid);
      proxies().erase(nid, aid);
    },
    // received from the BASP instance when receiving down_message
    [this](delete_atom, const node_id& nid, actor_id aid, error& fail_state) {
      auto lg = log::io::trace("nid = {}, aid = {}, fail_state = {}", nid, aid,
                               fail_state);
      proxies().erase(nid, aid, std::move(fail_state));
    },
    [this](unpublish_atom, const actor_addr& whom,
           uint16_t port) -> result<void> {
      auto lg = log::io::trace("whom = {}, port = {}", whom, port);
      auto cb = make_callback(
        [&](const strong_actor_ptr&, uint16_t x) { close(hdl_by_port(x)); });
      if (instance.remove_published_actor(whom, port, &cb) == 0)
        return sec::no_actor_published_at_port;
      return unit;
    },
    [this](close_atom, uint16_t port) -> result<void> {
      if (port == 0)
        return sec::cannot_close_invalid_port;
      // It is well-defined behavior to not have an actor published here,
      // hence the result can be ignored safely.
      instance.remove_published_actor(port, nullptr);
      auto res = close(hdl_by_port(port));
      if (res)
        return unit;
      return sec::cannot_close_invalid_port;
    },
    [this](get_atom,
           const node_id& x) -> result<node_id, std::string, uint16_t> {
      std::string addr;
      uint16_t port = 0;
      auto hdl = instance.tbl().lookup_direct(x);
      if (hdl) {
        addr = remote_addr(*hdl);
        port = remote_port(*hdl);
      }
      return {x, std::move(addr), port};
    },
    [this](tick_atom, actor_clock::time_point::rep scheduled_rep,
           timespan heartbeat_interval, timespan connection_timeout) {
      auto scheduled_tse = actor_clock::time_point::duration{scheduled_rep};
      auto scheduled = actor_clock::time_point{scheduled_tse};
      auto now = clock().now();
      if (now < scheduled) {
        log::io::warning("received tick before its time, reschedule");
        mail(tick_atom_v, scheduled.time_since_epoch().count(),
             heartbeat_interval, connection_timeout)
          .schedule(scheduled)
          .send(this);
        return;
      }
      auto next_tick = scheduled + heartbeat_interval;
      if (now >= next_tick) {
        log::io::error("Lagging a full heartbeat interval behind! "
                       "Interval too low or BASP actor overloaded! "
                       "Other nodes may disconnect.");
        while (now >= next_tick)
          next_tick += heartbeat_interval;

      } else if (now >= scheduled + (heartbeat_interval / 2)) {
        log::io::warning(
          "Lagging more than 50% of a heartbeat interval behind! "
          "Interval too low or BASP actor overloaded!");
      }
      // Send out heartbeats.
      instance.handle_heartbeat(context());
      // Check whether any node reached the disconnect timeout.
      for (auto i = ctx.begin(); i != ctx.end();) {
        if (i->second.last_seen + connection_timeout < now) {
          log::io::warning("Disconnect BASP node: reached connection timeout!");
          auto hdl = i->second.hdl;
          // connection_cleanup below calls ctx.erase, so we need to increase
          // the iterator now, before it gets invalidated.
          ++i;
          connection_cleanup(hdl, sec::connection_timeout);
          close(hdl);
        } else {
          ++i;
        }
      }
      // Schedule next tick.
      mail(tick_atom_v, next_tick.time_since_epoch().count(),
           heartbeat_interval, connection_timeout)
        .schedule(next_tick)
        .send(this);
    }};
}

proxy_registry* basp_broker::proxy_registry_ptr() {
  return &instance.proxies();
}

resumable::resume_result basp_broker::resume(execution_unit* ctx, size_t mt) {
  proxy_registry::current(&instance.proxies());
  auto guard = detail::scope_guard{[]() noexcept { //
    proxy_registry::current(nullptr);
  }};
  return super::resume(ctx, mt);
}

strong_actor_ptr basp_broker::make_proxy(node_id nid, actor_id aid) {
  auto lg = log::io::trace("nid = {}, aid = {}", nid, aid);
  CAF_ASSERT(nid != this_node());
  if (nid == none || aid == invalid_actor_id)
    return nullptr;
  auto mm = &system().middleman();
  // this member function is being called whenever we deserialize a
  // payload received from a remote node; if a remote node A sends
  // us a handle to a third node B, then we assume that A offers a route to B
  if (t_last_hop != nullptr && nid != *t_last_hop
      && instance.tbl().add_indirect(*t_last_hop, nid))
    mm->backend().dispatch([this, nid] { learned_new_node_indirectly(nid); });
  // we need to tell remote side we are watching this actor now;
  // use a direct route if possible, i.e., when talking to a third node
  // create proxy and add functor that will be called if we
  // receive a basp::down_message
  actor_config cfg;
  auto res = make_actor<forwarding_actor_proxy, strong_actor_ptr>(aid, nid,
                                                                  &(system()),
                                                                  cfg, this);
  strong_actor_ptr selfptr{ctrl()};
  res->get()->attach_functor([=](const error& rsn) {
    mm->backend().post([=] {
      // using res->id() instead of aid keeps this actor instance alive
      // until the original instance terminates, thus preventing subtle
      // bugs with attachables
      auto bptr = static_cast<basp_broker*>(selfptr->get());
      if (!bptr->getf(abstract_actor::is_terminated_flag))
        bptr->proxies().erase(nid, res->id(), rsn);
    });
  });
  return res;
}

void basp_broker::set_last_hop(node_id* ptr) {
  t_last_hop = ptr;
}

void basp_broker::finalize_handshake(const node_id& nid, actor_id aid,
                                     std::set<std::string>& sigs) {
  auto lg = log::io::trace("nid = {}, aid = {}, sigs = {}", nid, aid, sigs);
  CAF_ASSERT(this_context != nullptr);
  this_context->id = nid;
  auto& cb = this_context->callback;
  if (!cb)
    return;
  strong_actor_ptr ptr;
  // aid can be invalid when connecting to the default port of a node
  if (aid != invalid_actor_id) {
    if (nid == this_node()) {
      // connected to self
      ptr = actor_cast<strong_actor_ptr>(system().registry().get(aid));
      if (!ptr) {
        log::io::debug("actor not found: aid = {}", aid);
      }
    } else {
      ptr = namespace_.get_or_put(nid, aid);
      if (!ptr) {
        log::io::error("creating actor in finalize_handshake failed");
      }
    }
  }
  cb->deliver(nid, std::move(ptr), std::move(sigs));
  cb = std::nullopt;
}

void basp_broker::purge_state(const node_id& nid) {
  auto lg = log::io::trace("nid = {}", nid);
  // Destroy all proxies of the lost node.
  namespace_.erase(nid);
  // Cleanup all remaining references to the lost node.
  for (auto& kvp : monitored_actors)
    kvp.second.erase(nid);
}

void basp_broker::send_basp_down_message(const node_id& nid, actor_id aid,
                                         error rsn) {
  auto lg = log::io::trace("nid = {}, aid = {}, rsn = {}", nid, aid, rsn);
  auto path = instance.tbl().lookup(nid);
  if (!path) {
    log::io::info(
      "cannot send exit message for proxy, no route to host: nid = {}", nid);
    return;
  }
  instance.write_down_message(context(), get_buffer(path->hdl), nid, aid, rsn);
  instance.flush(*path);
}

void basp_broker::proxy_announced(const node_id& nid, actor_id aid) {
  auto lg = log::io::trace("nid = {}, aid = {}", nid, aid);
  // source node has created a proxy for one of our actors
  auto ptr = system().registry().get(aid);
  if (ptr == nullptr) {
    log::io::debug("kill proxy immediately");
    // kill immediately if actor has already terminated
    send_basp_down_message(nid, aid, exit_reason::unknown);
  } else {
    auto entry = ptr->address();
    auto i = monitored_actors.find(entry);
    if (i == monitored_actors.end()) {
      monitor(ptr);
      std::unordered_set<node_id> tmp{nid};
      monitored_actors.emplace(entry, std::move(tmp));
    } else {
      i->second.emplace(nid);
    }
  }
}

void basp_broker::handle_down_msg(down_msg& dm) {
  auto i = monitored_actors.find(dm.source);
  if (i == monitored_actors.end())
    return;
  for (auto& nid : i->second)
    send_basp_down_message(nid, dm.source.id(), dm.reason);
  monitored_actors.erase(i);
}

void basp_broker::emit_node_down_msg(const node_id& node, const error& reason) {
  auto i = node_observers.find(node);
  if (i == node_observers.end())
    return;
  for (const auto& observer : i->second)
    if (auto hdl = actor_cast<actor>(observer))
      anon_mail(node_down_msg{node, reason}).send(hdl);
  node_observers.erase(i);
}

void basp_broker::learned_new_node(const node_id& nid) {
  auto lg = log::io::trace("nid = {}", nid);
  if (spawn_servers.count(nid) > 0) {
    log::io::error("learned_new_node called for known node nid = {}", nid);
    return;
  }
  auto tmp = system().spawn<hidden>([=](event_based_actor* tself) -> behavior {
    auto lg = log::io::trace("");
    // terminate when receiving a down message
    tself->set_down_handler([=](down_msg& dm) {
      auto lg = log::io::trace("dm = {}", dm);
      tself->quit(std::move(dm.reason));
    });
    // skip messages until we receive the initial ok_atom
    tself->set_default_handler(skip);
    return {

      [=](ok_atom, const std::string& /* key == "info" */,
          const strong_actor_ptr& config_serv, const std::string& /* name */) {
        auto lg = log::io::trace("config_serv = {}", config_serv);
        // drop unexpected messages from this point on
        tself->set_default_handler(print_and_drop);
        if (!config_serv)
          return;
        tself->monitor(config_serv);
        tself->become([=](spawn_atom, std::string& type, message& args)
                        -> delegated<strong_actor_ptr, std::set<std::string>> {
          auto lg = log::io::trace("type = {}, args = {}", type, args);
          tself->delegate(actor_cast<actor>(std::move(config_serv)), get_atom_v,
                          std::move(type), std::move(args));
          return {};
        });
      },
      after(std::chrono::minutes(5)) >>
        [=] {
          log::io::info("no spawn server found: nid = {}", nid);
          tself->quit();
        }};
  });
  spawn_servers.emplace(nid, tmp);
  using namespace detail;
  auto tmp_ptr = actor_cast<strong_actor_ptr>(tmp);
  system().registry().put(tmp.id(), tmp_ptr);
  if (!instance.dispatch(context(), tmp_ptr, nid, basp::header::spawn_server_id,
                         basp::header::named_receiver_flag, make_message_id(),
                         make_message(sys_atom_v, get_atom_v, "info"))) {
    log::io::error(
      "learned_new_node called, but no route to remote node nid = {}", nid);
  }
}

void basp_broker::learned_new_node_directly(const node_id& nid,
                                            bool was_indirectly_before) {
  auto lg = log::io::trace("nid = {}", nid);
  if (!was_indirectly_before)
    learned_new_node(nid);
}

void basp_broker::learned_new_node_indirectly(const node_id& nid) {
  auto lg = log::io::trace("nid = {}", nid);
  learned_new_node(nid);
  if (!automatic_connections)
    return;
  // This member function gets only called once, after adding a new indirect
  // connection to the routing table; hence, spawning our helper here exactly
  // once and there is no need to track in-flight connection requests.
  using namespace detail;
  auto tmp = get_or(config(), "caf.middleman.attach-utility-actors", false)
               ? system().spawn<hidden>(connection_helper, this)
               : system().spawn<detached + hidden>(connection_helper, this);
  auto sender = actor_cast<strong_actor_ptr>(tmp);
  system().registry().put(sender->id(), sender);
  if (!instance.dispatch(context(), sender, nid, basp::header::config_server_id,
                         basp::header::named_receiver_flag, make_message_id(),
                         make_message(get_atom_v,
                                      "basp.default-connectivity-tcp"))) {
    log::io::error("learned_new_node_indirectly called, but no route to nid");
  }
}

void basp_broker::set_context(connection_handle hdl) {
  auto lg = log::io::trace("hdl = {}", hdl);
  auto now = clock().now();
  auto i = ctx.find(hdl);
  if (i == ctx.end()) {
    log::io::debug("create new BASP context: hdl = {}", hdl);
    basp::header hdr{basp::message_type::server_handshake,
                     0,
                     0,
                     0,
                     invalid_actor_id,
                     invalid_actor_id};
    i = ctx
          .emplace(hdl,
                   basp::endpoint_context{basp::await_header, hdr, hdl,
                                          node_id{}, 0, 0, std::nullopt, now})
          .first;
  } else {
    i->second.last_seen = now;
  }
  this_context = &i->second;
  t_last_hop = &i->second.id;
}

void basp_broker::connection_cleanup(connection_handle hdl, sec code) {
  auto lg = log::io::trace("hdl = {}, code = {}", hdl, code);
  // Remove handle from the routing table, notify all observers, and clean up
  // any node-specific state we might still have.
  if (auto nid = instance.tbl().erase_direct(hdl)) {
    emit_node_down_msg(nid, code);
    purge_state(nid);
  }
  // Remove the context for `hdl`, making sure clients receive an error in case
  // this connection was closed during handshake.
  auto i = ctx.find(hdl);
  if (i != ctx.end()) {
    auto& ref = i->second;
    CAF_ASSERT(i->first == ref.hdl);
    if (ref.callback) {
      log::io::debug("connection closed during handshake: code = {}", code);
      auto x = code != sec::none ? code : sec::disconnect_during_handshake;
      ref.callback->deliver(x);
    }
    ctx.erase(i);
  }
}

byte_buffer& basp_broker::get_buffer(connection_handle hdl) {
  return wr_buf(hdl);
}

void basp_broker::flush(connection_handle hdl) {
  super::flush(hdl);
}

void basp_broker::handle_heartbeat() {
  // nop
}

execution_unit* basp_broker::current_execution_unit() {
  return context();
}

strong_actor_ptr basp_broker::this_actor() {
  return ctrl();
}

} // namespace caf::io
