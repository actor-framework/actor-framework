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

#include <set>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <condition_variable>

#include "caf/locks.hpp"

#include "caf/all.hpp"
#include "caf/group.hpp"
#include "caf/message.hpp"
#include "caf/serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/group_manager.hpp"

namespace caf {

namespace {

using exclusive_guard = unique_lock<detail::shared_spinlock>;
using shared_guard = shared_lock<detail::shared_spinlock>;
using upgrade_guard = upgrade_lock<detail::shared_spinlock>;
using upgrade_to_unique_guard = upgrade_to_unique_lock<detail::shared_spinlock>;

class local_broker;
class local_group_module;

void await_all_locals_down(actor_system& sys, std::initializer_list<actor> xs) {
  CAF_LOG_TRACE("");
  scoped_actor self{sys, true};
  std::vector<actor> ys;
  for (auto& x : xs)
    if (x.node() == sys.node()) {
      self->send_exit(x, exit_reason::kill);
      ys.push_back(x);
    }
  self->wait_for(ys);
}

class local_group : public abstract_group {
public:
  void send_all_subscribers(const strong_actor_ptr& sender, const message& msg,
                            execution_unit* host) {
    CAF_LOG_TRACE(CAF_ARG(sender) << CAF_ARG(msg));
    shared_guard guard(mtx_);
    for (auto& s : subscribers_)
      s->enqueue(sender, invalid_message_id, msg, host);
  }

  void enqueue(strong_actor_ptr sender, message_id, message msg,
               execution_unit* host) override {
    CAF_LOG_TRACE(CAF_ARG(sender) << CAF_ARG(msg));
    send_all_subscribers(sender, msg, host);
    broker_->enqueue(sender, invalid_message_id, msg, host);
  }

  std::pair<bool, size_t> add_subscriber(strong_actor_ptr who) {
    CAF_LOG_TRACE(CAF_ARG(who));
    if (! who)
      return {false, subscribers_.size()};
    exclusive_guard guard(mtx_);
    auto res = subscribers_.emplace(std::move(who)).second;
    return {res, subscribers_.size()};
  }

  std::pair<bool, size_t> erase_subscriber(const actor_control_block* who) {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    exclusive_guard guard(mtx_);
    auto cmp = [](const strong_actor_ptr& lhs, const actor_control_block* rhs) {
      return actor_addr::compare(lhs.get(), rhs) < 0;
    };
    auto e = subscribers_.end();
    auto i = std::lower_bound(subscribers_.begin(), e, who, cmp);
    if (i == e || actor_addr::compare(i->get(), who) != 0)
      return {false, subscribers_.size()};
    subscribers_.erase(i);
    return {true, subscribers_.size()};
  }

  bool subscribe(strong_actor_ptr who) override {
    CAF_LOG_TRACE(CAF_ARG(who));
    if (add_subscriber(std::move(who)).first)
      return true;
    return false;
  }

  void unsubscribe(const actor_control_block* who) override {
    CAF_LOG_TRACE(CAF_ARG(who));
    erase_subscriber(who);
  }

  void save(serializer& sink) const override;

  void stop() override {
    CAF_LOG_TRACE("");
    await_all_locals_down(system(), {broker_});
    actor tmp{std::move(broker_)}; // manually break cycle
  }

  const actor& broker() const {
    return broker_;
  }

  local_group(actor_system& sys, optional<actor> local_broker,
              local_group_module* mod, std::string id, const node_id& nid);

  ~local_group();

protected:
  detail::shared_spinlock mtx_;
  std::set<strong_actor_ptr> subscribers_;
  actor broker_;
};

using local_group_ptr = intrusive_ptr<local_group>;

class local_broker : public event_based_actor {
public:
  explicit local_broker(actor_config& cfg, local_group_ptr g)
      : event_based_actor(cfg),
        group_(std::move(g)) {
    // nop
  }

  void on_exit() override {
    acquaintances_.clear();
    group_.reset();
  }

  const char* name() const override {
    return "local_broker";
  }

  behavior make_behavior() override {
    CAF_LOG_TRACE("");
    // instead of dropping "unexpected" messages,
    // we simply forward them to our acquaintances
    auto fwd = [=](local_actor*, const type_erased_tuple* ptr) -> result<message> {
      send_to_acquaintances(message::from(ptr));
      return message{};
    };
    set_default_handler(fwd);
    set_down_handler([=](down_msg& dm) {
      CAF_LOG_TRACE(CAF_ARG(dm));
      auto first = acquaintances_.begin();
      auto last = acquaintances_.end();
      auto i = std::find_if(first, last, [&](const actor& a) {
        return a == dm.source;
      });
      if (i != last)
        acquaintances_.erase(i);
    });
    // return behavior
    return {
      [=](join_atom, const actor& other) {
        CAF_LOG_TRACE(CAF_ARG(other));
        if (acquaintances_.insert(other).second) {
          monitor(other);
        }
      },
      [=](leave_atom, const actor& other) {
        CAF_LOG_TRACE(CAF_ARG(other));
        acquaintances_.erase(other);
        if (acquaintances_.erase(other) > 0)
          demonitor(other);
      },
      [=](forward_atom, const message& what) {
        CAF_LOG_TRACE(CAF_ARG(what));
        // local forwarding
        group_->send_all_subscribers(current_element_->sender, what, context());
        // forward to all acquaintances
        send_to_acquaintances(what);
      }
    };
  }

private:
  void send_to_acquaintances(const message& what) {
    // send to all remote subscribers
    auto src = current_element_->sender;
    CAF_LOG_DEBUG(CAF_ARG(acquaintances_.size())
                  << CAF_ARG(src) << CAF_ARG(what));
    for (auto& acquaintance : acquaintances_)
      acquaintance->enqueue(src, invalid_message_id, what, context());
  }

  local_group_ptr group_;
  std::set<actor> acquaintances_;
};

// Send a join message to the original group if a proxy
// has local subscriptions and a "LEAVE" message to the original group
// if there's no subscription left.

class local_group_proxy;

using local_group_proxy_ptr = intrusive_ptr<local_group_proxy>;

class proxy_broker : public event_based_actor {
public:
  proxy_broker(actor_config& cfg, local_group_proxy_ptr grp)
      : event_based_actor(cfg),
        group_(std::move(grp)) {
    CAF_LOG_TRACE("");
  }

  behavior make_behavior();

  void on_exit() {
    group_.reset();
  }

private:
  local_group_proxy_ptr group_;
};

class local_group_proxy : public local_group {
public:
  using super = local_group;

  template <class... Ts>
  local_group_proxy(actor_system& sys, actor remote_broker, Ts&&... xs)
      : super(sys, std::move(remote_broker), std::forward<Ts>(xs)...),
        proxy_broker_{sys.spawn<proxy_broker, hidden>(this)},
        monitor_{sys.spawn<hidden>(broker_monitor_actor, this)} {
    // nop
  }

  bool subscribe(strong_actor_ptr who) override {
    CAF_LOG_TRACE(CAF_ARG(who));
    auto res = add_subscriber(std::move(who));
    if (res.first) {
      // join remote source
      if (res.second == 1)
        anon_send(broker_, join_atom::value, proxy_broker_);
      return true;
    }
    CAF_LOG_WARNING("actor already joined group");
    return false;
  }

  void unsubscribe(const actor_control_block* who) override {
    CAF_LOG_TRACE(CAF_ARG(who));
    auto res = erase_subscriber(who);
    if (res.first && res.second == 0) {
      // leave the remote source,
      // because there's no more subscriber on this node
      anon_send(broker_, leave_atom::value, proxy_broker_);
    }
  }

  void enqueue(strong_actor_ptr sender, message_id mid,
               message msg, execution_unit* eu) override {
    CAF_LOG_TRACE(CAF_ARG(sender) << CAF_ARG(mid) << CAF_ARG(msg));
    // forward message to the broker
    broker_->enqueue(std::move(sender), mid,
                     make_message(forward_atom::value, std::move(msg)), eu);
  }

  void stop() override {
    CAF_LOG_TRACE("");
    await_all_locals_down(system_, {monitor_, proxy_broker_, broker_});
    destroy(monitor_);
    destroy(proxy_broker_);
    destroy(broker_);
  }

private:
  static behavior broker_monitor_actor(event_based_actor* self,
                                       local_group_proxy* grp) {
    CAF_LOG_TRACE("");
    self->monitor(grp->broker_);
    self->set_down_handler([=](down_msg& down) {
      CAF_LOG_TRACE(CAF_ARG(down));
      auto msg = make_message(group_down_msg{group(grp)});
      grp->send_all_subscribers(self->ctrl(), std::move(msg),
                                self->context());
      self->quit(down.reason);
    });
    return {
      [] {
        // nop
      }
    };
  }

  actor proxy_broker_;
  actor monitor_;
};

behavior proxy_broker::make_behavior() {
  CAF_LOG_TRACE("");
  // instead of dropping "unexpected" messages,
  // we simply forward them to our acquaintances
  auto fwd = [=](local_actor*, const type_erased_tuple* x) -> result<message> {
    group_->send_all_subscribers(current_element_->sender, message::from(x),
                                 context());
    return message{};
  };
  set_default_handler(fwd);
  // return dummy behavior
  return {
    [] {
      // nop
    }
  };
}

class local_group_module : public abstract_group::module {
public:
  using super = abstract_group::module;

  local_group_module(actor_system& sys) : super(sys, "local") {
    CAF_LOG_TRACE("");
  }

  group get(const std::string& identifier) override {
    CAF_LOG_TRACE(CAF_ARG(identifier));
    upgrade_guard guard(instances_mtx_);
    auto i = instances_.find(identifier);
    if (i != instances_.end())
      return {i->second};
    auto tmp = make_counted<local_group>(system(), none, this, identifier,
                                         system().node());
    upgrade_to_unique_guard uguard(guard);
    auto p = instances_.emplace(identifier, tmp);
    auto result = p.first->second;
    uguard.unlock();
    // someone might preempt us
    if (result != tmp)
      tmp->stop();
    return {result};
  }

  group load(deserializer& source) override {
    CAF_LOG_TRACE("");
    // deserialize identifier and broker
    std::string identifier;
    strong_actor_ptr broker_ptr;
    source >> identifier >> broker_ptr;
    CAF_LOG_DEBUG(CAF_ARG(identifier) << CAF_ARG(broker_ptr));
    if (! broker_ptr)
      return invalid_group;
    auto broker = actor_cast<actor>(broker_ptr);
    if (broker->node() == system().node())
      return this->get(identifier);
    upgrade_guard guard(proxies_mtx_);
    auto i = proxies_.find(broker);
    if (i != proxies_.end())
      return {i->second};
    local_group_ptr tmp = make_counted<local_group_proxy>(system(), broker,
                                                          this, identifier,
                                                          broker->node());
    upgrade_to_unique_guard uguard(guard);
    auto p = proxies_.emplace(broker, tmp);
    // someone might preempt us
    return {p.first->second};
  }

  void save(const local_group* ptr, serializer& sink) const {
    CAF_ASSERT(ptr != nullptr);
    CAF_LOG_TRACE("");
    sink << ptr->identifier() << actor_cast<strong_actor_ptr>(ptr->broker());
  }

  void stop() override {
    CAF_LOG_TRACE("");
    std::map<std::string, local_group_ptr> imap;
    std::map<actor, local_group_ptr> pmap;
    { // critical section
      exclusive_guard guard1{instances_mtx_};
      exclusive_guard guard2{proxies_mtx_};
      imap.swap(instances_);
      pmap.swap(proxies_);
    }
    for (auto& kvp : imap)
      kvp.second->stop();
    for (auto& kvp : pmap)
      kvp.second->stop();
  }

private:
  detail::shared_spinlock instances_mtx_;
  std::map<std::string, local_group_ptr> instances_;
  detail::shared_spinlock proxies_mtx_;
  std::map<actor, local_group_ptr> proxies_;
};

local_group::local_group(actor_system& sys, optional<actor> lb,
                         local_group_module* mod, std::string id,
                         const node_id& nid)
    : abstract_group(sys, mod, std::move(id), nid),
      broker_(lb ? *lb : sys.spawn<local_broker, hidden>(this)) {
  CAF_LOG_TRACE(CAF_ARG(id) << CAF_ARG(nid));
}

local_group::~local_group() {
  // nop
}

void local_group::save(serializer& sink) const {
  CAF_LOG_TRACE("");
  // this cast is safe, because the only available constructor accepts
  // local_group_module* as module pointer
  static_cast<local_group_module*>(module_)->save(this, sink);
}

std::atomic<size_t> s_ad_hoc_id;

} // namespace <anonymous>

void group_manager::start() {
  CAF_LOG_TRACE("");
}

void group_manager::stop() {
  CAF_LOG_TRACE("");
  modules_map mm;
  { // critical section
    std::lock_guard<std::mutex> guard(mmap_mtx_);
    mm.swap(mmap_);
  }
  for (auto& kvp : mm) {
    kvp.second->stop();
  }
}

group_manager::~group_manager() {
  // nop
}

group_manager::group_manager(actor_system& sys) : system_(sys) {
  CAF_LOG_TRACE("");
  abstract_group::unique_module_ptr ptr{new local_group_module(sys)};
  mmap_.emplace(std::string("local"), std::move(ptr));
}

group group_manager::anonymous() {
  CAF_LOG_TRACE("");
  std::string id = "__#";
  id += std::to_string(++s_ad_hoc_id);
  return get_module("local")->get(id);
}

expected<group> group_manager::get(const std::string& module_name,
                                   const std::string& group_identifier) {
  CAF_LOG_TRACE(CAF_ARG(module_name) << CAF_ARG(group_identifier));
  auto mod = get_module(module_name);
  if (mod) {
    return mod->get(group_identifier);
  }
  std::string error_msg = "no module named \"";
  error_msg += module_name;
  error_msg += "\" found";
  return make_error(sec::no_such_group_module, std::move(error_msg));
}

group group_manager::get_local(const std::string& group_identifier) {
  // guaranteed to never return an error
  return *get("local", group_identifier);
}

void group_manager::add_module(std::unique_ptr<abstract_group::module> mptr) {
  CAF_LOG_TRACE("");
  if (! mptr)
    return;
  auto& mname = mptr->name();
  { // lifetime scope of guard
    std::lock_guard<std::mutex> guard(mmap_mtx_);
    if (mmap_.emplace(mname, std::move(mptr)).second)
      return; // success; don't throw
  }
  std::string error_msg = "module name \"";
  error_msg += mname;
  error_msg += "\" already defined";
  CAF_RAISE_ERROR(std::move(error_msg));
}

abstract_group::module* group_manager::get_module(const std::string& mname) {
  CAF_LOG_TRACE(CAF_ARG(mname));
  std::lock_guard<std::mutex> guard(mmap_mtx_);
  auto i = mmap_.find(mname);
  return (i != mmap_.end()) ? i->second.get() : nullptr;
}

} // namespace caf
