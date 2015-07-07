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
#include "caf/to_string.hpp"
#include "caf/message.hpp"
#include "caf/serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/event_based_actor.hpp"

#include "caf/detail/group_manager.hpp"

namespace caf {
namespace detail {

namespace {

using exclusive_guard = unique_lock<detail::shared_spinlock>;
using shared_guard = shared_lock<detail::shared_spinlock>;
using upgrade_guard = upgrade_lock<detail::shared_spinlock>;
using upgrade_to_unique_guard = upgrade_to_unique_lock<detail::shared_spinlock>;

class local_broker;
class local_group_module;

void await_all_locals_down(std::initializer_list<actor> xs) {
  CAF_LOGF_TRACE("");
  size_t awaited_down_msgs = 0;
  scoped_actor self{true};
  for (auto& x : xs) {
    if (x != invalid_actor && ! x.is_remote()) {
      self->monitor(x);
      self->send_exit(x, exit_reason::user_shutdown);
      ++awaited_down_msgs;
    }
  }
  for (size_t i = 0; i < awaited_down_msgs; ++i) {
    self->receive(
      [](const down_msg&) {
        // nop
      },
      after(std::chrono::seconds(1)) >> [&] {
        throw std::logic_error("at least one actor did not quit within 1s");
      }
    );
  }
}

class local_group : public abstract_group {
public:
  void send_all_subscribers(const actor_addr& sender, const message& msg,
                            execution_unit* host) {
    CAF_LOG_TRACE(CAF_TARG(sender, to_string) << ", "
                  << CAF_TARG(msg, to_string));
    shared_guard guard(mtx_);
    for (auto& s : subscribers_) {
      actor_cast<abstract_actor_ptr>(s)->enqueue(sender, invalid_message_id,
                                                 msg, host);
    }
  }

  void enqueue(const actor_addr& sender, message_id, message msg,
               execution_unit* host) override {
    CAF_LOG_TRACE(CAF_TARG(sender, to_string) << ", "
                  << CAF_TARG(msg, to_string));
    send_all_subscribers(sender, msg, host);
    broker_->enqueue(sender, invalid_message_id, msg, host);
  }

  std::pair<bool, size_t> add_subscriber(const actor_addr& who) {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    exclusive_guard guard(mtx_);
    if (who && subscribers_.insert(who).second) {
      return {true, subscribers_.size()};
    }
    return {false, subscribers_.size()};
  }

  std::pair<bool, size_t> erase_subscriber(const actor_addr& who) {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    exclusive_guard guard(mtx_);
    auto success = subscribers_.erase(who) > 0;
    return {success, subscribers_.size()};
  }

  attachable_ptr subscribe(const actor_addr& who) override {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    if (add_subscriber(who).first) {
      return subscription::make(this);
    }
    return {};
  }

  void unsubscribe(const actor_addr& who) override {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    erase_subscriber(who);
  }

  void serialize(serializer* sink);

  void stop() override {
    CAF_LOG_TRACE("");
    await_all_locals_down({broker_});
    broker_ = invalid_actor;
  }

  const actor& broker() const {
    return broker_;
  }

  local_group(bool spawn_local_broker, local_group_module* mod, std::string id);

  ~local_group();

protected:
  detail::shared_spinlock mtx_;
  std::set<actor_addr> subscribers_;
  actor broker_;
};

using local_group_ptr = intrusive_ptr<local_group>;

class local_broker : public event_based_actor {
public:
  explicit local_broker(local_group_ptr g) : group_(std::move(g)) {
    // nop
  }

  void on_exit() {
    acquaintances_.clear();
    group_.reset();
  }

  behavior make_behavior() override {
    return {
      [=](join_atom, const actor& other) {
        CAF_LOG_TRACE(CAF_TSARG(other));
        if (other && acquaintances_.insert(other).second) {
          monitor(other);
        }
      },
      [=](leave_atom, const actor& other) {
        CAF_LOG_TRACE(CAF_TSARG(other));
        if (other && acquaintances_.erase(other) > 0) {
          demonitor(other);
        }
      },
      [=](forward_atom, const message& what) {
        CAF_LOG_TRACE(CAF_TSARG(what));
        // local forwarding
        group_->send_all_subscribers(current_sender(), what, host());
        // forward to all acquaintances
        send_to_acquaintances(what);
      },
      [=](const down_msg&) {
        auto sender = current_sender();
        CAF_LOG_TRACE(CAF_TSARG(sender));
        if (sender) {
          auto first = acquaintances_.begin();
          auto last = acquaintances_.end();
          auto i = std::find_if(first, last, [=](const actor& a) {
            return a == sender;
          });
          if (i != last) {
            acquaintances_.erase(i);
          }
        }
      },
      others >> [=] {
        auto msg = current_message();
        CAF_LOG_TRACE(CAF_TSARG(msg));
        send_to_acquaintances(msg);
      }
    };
  }

private:
  void send_to_acquaintances(const message& what) {
    // send to all remote subscribers
    auto sender = current_sender();
    CAF_LOG_DEBUG("forward message to " << acquaintances_.size()
                  << " acquaintances; " << CAF_TSARG(sender) << ", "
                  << CAF_TSARG(what));
    for (auto& acquaintance : acquaintances_) {
      acquaintance->enqueue(sender, invalid_message_id, what, host());
    }
  }

  local_group_ptr group_;
  std::set<actor> acquaintances_;
};

// Send a join message to the original group if a proxy
// has local subscriptions and a "LEAVE" message to the original group
// if there's no subscription left.

class proxy_broker;

class local_group_proxy : public local_group {
public:
  using super = local_group;

  template <class... Ts>
  local_group_proxy(actor remote_broker, Ts&&... xs)
      : super(false, std::forward<Ts>(xs)...) {
    CAF_ASSERT(broker_ == invalid_actor);
    CAF_ASSERT(remote_broker != invalid_actor);
    broker_ = std::move(remote_broker);
    proxy_broker_ = spawn<proxy_broker, hidden>(this);
    monitor_ = spawn(broker_monitor_actor, this);
  }

  attachable_ptr subscribe(const actor_addr& who) override {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    auto res = add_subscriber(who);
    if (res.first) {
      if (res.second == 1) {
        // join the remote source
        anon_send(broker_, join_atom::value, proxy_broker_);
      }
      return subscription::make(this);
    }
    CAF_LOG_WARNING("actor already joined group");
    return {};
  }

  void unsubscribe(const actor_addr& who) override {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    auto res = erase_subscriber(who);
    if (res.first && res.second == 0) {
      // leave the remote source,
      // because there's no more subscriber on this node
      anon_send(broker_, leave_atom::value, proxy_broker_);
    }
  }

  void enqueue(const actor_addr& sender, message_id mid, message msg,
               execution_unit* eu) override {
    // forward message to the broker
    broker_->enqueue(sender, mid,
                      make_message(forward_atom::value, std::move(msg)),
                      eu);
  }

  void stop() override {
    CAF_LOG_TRACE("");
    await_all_locals_down({monitor_, proxy_broker_, broker_});
    monitor_ = invalid_actor;
    proxy_broker_ = invalid_actor;
    broker_ = invalid_actor;
  }

private:
  static behavior broker_monitor_actor(event_based_actor* self,
                                       local_group_proxy* grp) {
    self->monitor(grp->broker_);
    return {
      [=](const down_msg& down) {
        auto msg = make_message(group_down_msg{group(grp)});
        grp->send_all_subscribers(self->address(), std::move(msg),
                                  self->host());
        self->quit(down.reason);
      }
    };
  }

  actor proxy_broker_;
  actor monitor_;
};

using local_group_proxy_ptr = intrusive_ptr<local_group_proxy>;

class proxy_broker : public event_based_actor {
public:
  explicit proxy_broker(local_group_proxy_ptr grp) : group_(std::move(grp)) {
    // nop
  }

  behavior make_behavior() {
    return {
      others >> [=] {
        group_->send_all_subscribers(current_sender(), current_message(),
                                      host());
      }
    };
  }

  void on_exit() {
    group_.reset();
  }

private:
  local_group_proxy_ptr group_;
};

class local_group_module : public abstract_group::module {
public:
  using super = abstract_group::module;

  local_group_module()
      : super("local"), actor_utype_(uniform_typeid<actor>()) {
    // nop
  }

  group get(const std::string& identifier) override {
    upgrade_guard guard(instances_mtx_);
    auto i = instances_.find(identifier);
    if (i != instances_.end()) {
      return {i->second};
    }
    auto tmp = make_counted<local_group>(true, this, identifier);
    upgrade_to_unique_guard uguard(guard);
    auto p = instances_.emplace(identifier, tmp);
    auto result = p.first->second;
    uguard.unlock();
    // someone might preempt us
    if (result != tmp) {
      tmp->stop();
    }
    return {result};
  }

  group deserialize(deserializer* source) override {
    // deserialize {identifier, process_id, node_id}
    auto identifier = source->read<std::string>();
    // deserialize broker
    actor broker;
    actor_utype_->deserialize(&broker, source);
    if (! broker) {
      return invalid_group;
    }
    if (! broker->is_remote()) {
      return this->get(identifier);
    }
    upgrade_guard guard(proxies_mtx_);
    auto i = proxies_.find(broker);
    if (i != proxies_.end()) {
      return {i->second};
    }
    local_group_ptr tmp = make_counted<local_group_proxy>(broker, this,
                                                          identifier);
    upgrade_to_unique_guard uguard(guard);
    auto p = proxies_.emplace(broker, tmp);
    // someone might preempt us
    return {p.first->second};
  }

  void serialize(local_group* ptr, serializer* sink) {
    // serialize identifier & broker
    sink->write_value(ptr->identifier());
    CAF_ASSERT(ptr->broker() != invalid_actor);
    actor_utype_->serialize(&ptr->broker(), sink);
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
    for (auto& kvp : imap) {
      kvp.second->stop();
    }
    for (auto& kvp : pmap) {
      kvp.second->stop();
    }
  }

private:
  const uniform_type_info* actor_utype_;
  detail::shared_spinlock instances_mtx_;
  std::map<std::string, local_group_ptr> instances_;
  detail::shared_spinlock proxies_mtx_;
  std::map<actor, local_group_ptr> proxies_;
};

local_group::local_group(bool do_spawn, local_group_module* mod, std::string id)
    : abstract_group(mod, std::move(id)) {
  if (do_spawn) {
    broker_ = spawn<local_broker, hidden>(this);
  }
  // else: derived class spawns broker_
}

local_group::~local_group() {
  // nop
}

void local_group::serialize(serializer* sink) {
  // this cast is safe, because the only available constructor accepts
  // local_group_module* as module pointer
  static_cast<local_group_module*>(module_)->serialize(this, sink);
}

std::atomic<size_t> s_ad_hoc_id;

} // namespace <anonymous>

void group_manager::stop() {
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

group_manager::group_manager() {
  abstract_group::unique_module_ptr ptr{new local_group_module};
  mmap_.emplace(std::string("local"), std::move(ptr));
}

group group_manager::anonymous() {
  std::string id = "__#";
  id += std::to_string(++s_ad_hoc_id);
  return get_module("local")->get(id);
}

group group_manager::get(const std::string& module_name,
                         const std::string& group_identifier) {
  auto mod = get_module(module_name);
  if (mod) {
    return mod->get(group_identifier);
  }
  std::string error_msg = "no module named \"";
  error_msg += module_name;
  error_msg += "\" found";
  throw std::logic_error(error_msg);
}

void group_manager::add_module(std::unique_ptr<abstract_group::module> mptr) {
  if (! mptr) {
    return;
  }
  auto& mname = mptr->name();
  { // lifetime scope of guard
    std::lock_guard<std::mutex> guard(mmap_mtx_);
    if (mmap_.emplace(mname, std::move(mptr)).second) {
      return; // success; don't throw
    }
  }
  std::string error_msg = "module name \"";
  error_msg += mname;
  error_msg += "\" already defined";
  throw std::logic_error(error_msg);
}

abstract_group::module* group_manager::get_module(const std::string& mname) {
  std::lock_guard<std::mutex> guard(mmap_mtx_);
  auto i = mmap_.find(mname);
  return (i != mmap_.end()) ? i->second.get() : nullptr;
}

} // namespace detail
} // namespace caf
