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

class local_group : public abstract_group {
 public:
  void send_all_subscribers(const actor_addr& sender, const message& msg,
                            execution_unit* host) {
    CAF_LOG_TRACE(CAF_TARG(sender, to_string) << ", "
                  << CAF_TARG(msg, to_string));
    shared_guard guard(m_mtx);
    for (auto& s : m_subscribers) {
      actor_cast<abstract_actor_ptr>(s)->enqueue(sender, invalid_message_id,
                                                 msg, host);
    }
  }

  void enqueue(const actor_addr& sender, message_id, message msg,
               execution_unit* host) override {
    CAF_LOG_TRACE(CAF_TARG(sender, to_string) << ", "
                  << CAF_TARG(msg, to_string));
    send_all_subscribers(sender, msg, host);
    m_broker->enqueue(sender, invalid_message_id, msg, host);
  }

  std::pair<bool, size_t> add_subscriber(const actor_addr& who) {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    exclusive_guard guard(m_mtx);
    if (who && m_subscribers.insert(who).second) {
      return {true, m_subscribers.size()};
    }
    return {false, m_subscribers.size()};
  }

  std::pair<bool, size_t> erase_subscriber(const actor_addr& who) {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    exclusive_guard guard(m_mtx);
    auto success = m_subscribers.erase(who) > 0;
    return {success, m_subscribers.size()};
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

  const actor& broker() const {
    return m_broker;
  }

  local_group(bool spawn_local_broker, local_group_module* mod, std::string id);

 protected:
  detail::shared_spinlock m_mtx;
  std::set<actor_addr> m_subscribers;
  actor m_broker;
};

using local_group_ptr = intrusive_ptr<local_group>;

class local_broker : public event_based_actor {
 public:
  local_broker(local_group_ptr g) : m_group(std::move(g)) {
    // nop
  }

  behavior make_behavior() override {
    return {
      on(atom("JOIN"), arg_match) >> [=](const actor& other) {
        CAF_LOGC_TRACE("caf::local_broker", "init$JOIN",
                       CAF_TARG(other, to_string));
        if (other && m_acquaintances.insert(other).second) {
          monitor(other);
        }
      },
      on(atom("LEAVE"), arg_match) >> [=](const actor& other) {
        CAF_LOGC_TRACE("caf::local_broker", "init$LEAVE",
                       CAF_TARG(other, to_string));
        if (other && m_acquaintances.erase(other) > 0) {
          demonitor(other);
        }
      },
      on(atom("_Forward"), arg_match) >> [=](const message& what) {
        CAF_LOGC_TRACE("caf::local_broker", "init$FORWARD",
                       CAF_TARG(what, to_string));
        // local forwarding
        m_group->send_all_subscribers(last_sender(), what, host());
        // forward to all acquaintances
        send_to_acquaintances(what);
      },
      [=](const down_msg&) {
        auto sender = last_sender();
        CAF_LOGC_TRACE("caf::local_broker", "init$DOWN",
                       CAF_TARG(sender, to_string));
        if (sender) {
          auto first = m_acquaintances.begin();
          auto last = m_acquaintances.end();
          auto i = std::find_if(first, last, [=](const actor& a) {
            return a == sender;
          });
          if (i != last) {
            m_acquaintances.erase(i);
          }
        }
      },
      others() >> [=] {
        auto msg = last_dequeued();
        CAF_LOGC_TRACE("caf::local_broker", "init$others",
                       CAF_TARG(msg, to_string));
        send_to_acquaintances(msg);
      }
    };
  }

 private:
  void send_to_acquaintances(const message& what) {
    // send to all remote subscribers
    auto sender = last_sender();
    CAF_LOG_DEBUG("forward message to " << m_acquaintances.size()
                  << " acquaintances; " << CAF_TSARG(sender) << ", "
                  << CAF_TSARG(what));
    for (auto& acquaintance : m_acquaintances) {
      acquaintance->enqueue(sender, invalid_message_id, what, host());
    }
  }

  local_group_ptr m_group;
  std::set<actor> m_acquaintances;

};

// Send a "JOIN" message to the original group if a proxy
// has local subscriptions and a "LEAVE" message to the original group
// if there's no subscription left.

class proxy_broker;

class local_group_proxy : public local_group {
 public:
  using super = local_group;

  template <class... Ts>
  local_group_proxy(actor remote_broker, Ts&&... args)
      : super(false, std::forward<Ts>(args)...) {
    CAF_REQUIRE(m_broker == invalid_actor);
    CAF_REQUIRE(remote_broker != invalid_actor);
    m_broker = std::move(remote_broker);
    m_proxy_broker = spawn<proxy_broker, hidden>(this);
  }

  attachable_ptr subscribe(const actor_addr& who) override {
    CAF_LOG_TRACE(""); // serializing who would cause a deadlock
    auto res = add_subscriber(who);
    if (res.first) {
      if (res.second == 1) {
        // join the remote source
        anon_send(m_broker, atom("JOIN"), m_proxy_broker);
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
      anon_send(m_broker, atom("LEAVE"), m_proxy_broker);
    }
  }

  void enqueue(const actor_addr& sender, message_id mid, message msg,
               execution_unit* eu) override {
    // forward message to the broker
    m_broker->enqueue(sender, mid,
                      make_message(atom("_Forward"), std::move(msg)), eu);
  }

 private:
  actor m_proxy_broker;
};

using local_group_proxy_ptr = intrusive_ptr<local_group_proxy>;

class proxy_broker : public event_based_actor {
 public:
  proxy_broker(local_group_proxy_ptr grp) : m_group(std::move(grp)) {
    // nop
  }

  behavior make_behavior() {
    return {
      others() >> [=] {
        m_group->send_all_subscribers(last_sender(), last_dequeued(), host());
      }
    };
  }

 private:
  local_group_proxy_ptr m_group;
};

class local_group_module : public abstract_group::module {
 public:
  using super = abstract_group::module;
  local_group_module()
      : super("local"), m_actor_utype(uniform_typeid<actor>()) {
    // nop
  }

  group get(const std::string& identifier) override {
    upgrade_guard guard(m_instances_mtx);
    auto i = m_instances.find(identifier);
    if (i != m_instances.end()) {
      return {i->second};
    } else {
      auto tmp = make_counted<local_group>(true, this, identifier);
      { // lifetime scope of uguard
        upgrade_to_unique_guard uguard(guard);
        auto p = m_instances.insert(make_pair(identifier, tmp));
        // someone might preempt us
        return {p.first->second};
      }
    }
  }

  group deserialize(deserializer* source) override {
    // deserialize {identifier, process_id, node_id}
    auto identifier = source->read<std::string>();
    // deserialize broker
    actor broker;
    m_actor_utype->deserialize(&broker, source);
    if (!broker) {
      return invalid_group;
    }
    if (!broker->is_remote()) {
      return this->get(identifier);
    }
    upgrade_guard guard(m_proxies_mtx);
    auto i = m_proxies.find(broker);
    if (i != m_proxies.end()) {
      return {i->second};
    }
    local_group_ptr tmp{new local_group_proxy{broker, this, identifier}};
    upgrade_to_unique_guard uguard(guard);
    auto p = m_proxies.insert(std::make_pair(broker, tmp));
    // someone might preempt us
    return {p.first->second};
  }

  void serialize(local_group* ptr, serializer* sink) {
    // serialize identifier & broker
    sink->write_value(ptr->identifier());
    CAF_REQUIRE(ptr->broker() != invalid_actor);
    m_actor_utype->serialize(&ptr->broker(), sink);
  }

 private:
  const uniform_type_info* m_actor_utype;
  detail::shared_spinlock m_instances_mtx;
  std::map<std::string, local_group_ptr> m_instances;
  detail::shared_spinlock m_proxies_mtx;
  std::map<actor, local_group_ptr> m_proxies;
};

local_group::local_group(bool do_spawn, local_group_module* mod, std::string id)
    : abstract_group(mod, std::move(id)) {
  if (do_spawn) {
    m_broker = spawn<local_broker, hidden>(this);
  }
}

void local_group::serialize(serializer* sink) {
  // this cast is safe, because the only available constructor accepts
  // local_group_module* as module pointer
  static_cast<local_group_module*>(m_module)->serialize(this, sink);
}

std::atomic<size_t> m_ad_hoc_id;

} // namespace <anonymous>

group_manager::~group_manager() {
  // nop
}

group_manager::group_manager() {
  abstract_group::unique_module_ptr ptr{new local_group_module};
  m_mmap.insert(std::make_pair(std::string("local"), std::move(ptr)));
}

group group_manager::anonymous() {
  std::string id = "__#";
  id += std::to_string(++m_ad_hoc_id);
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
  if (!mptr) {
    return;
  }
  auto& mname = mptr->name();
  { // lifetime scope of guard
    std::lock_guard<std::mutex> guard(m_mmap_mtx);
    if (m_mmap.insert(std::make_pair(mname, std::move(mptr))).second) {
      return; // success; don't throw
    }
  }
  std::string error_msg = "module name \"";
  error_msg += mname;
  error_msg += "\" already defined";
  throw std::logic_error(error_msg);
}

abstract_group::module* group_manager::get_module(const std::string& mname) {
  std::lock_guard<std::mutex> guard(m_mmap_mtx);
  auto i = m_mmap.find(mname);
  return (i != m_mmap.end()) ? i->second.get() : nullptr;
}

} // namespace detail
} // namespace caf
