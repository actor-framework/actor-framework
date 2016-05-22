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

#include "caf/actor_registry.hpp"

#include <mutex>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "caf/sec.hpp"
#include "caf/locks.hpp"
#include "caf/logger.hpp"
#include "caf/exception.hpp"
#include "caf/actor_cast.hpp"
#include "caf/attachable.hpp"
#include "caf/exit_reason.hpp"
#include "caf/actor_system.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/uniform_type_info_map.hpp"

#include "caf/scheduler/detached_threads.hpp"

#include "caf/detail/shared_spinlock.hpp"

namespace caf {

namespace {

using exclusive_guard = unique_lock<detail::shared_spinlock>;
using shared_guard = shared_lock<detail::shared_spinlock>;

} // namespace <anonymous>

actor_registry::~actor_registry() {
  // nop
}

actor_registry::actor_registry(actor_system& sys) : running_(0), system_(sys) {
  // nop
}

strong_actor_ptr actor_registry::get(actor_id key) const {
  shared_guard guard(instances_mtx_);
  auto i = entries_.find(key);
  if (i != entries_.end())
    return i->second;
  CAF_LOG_DEBUG("key invalid, assume actor no longer exists:" << CAF_ARG(key));
  return nullptr;
}

void actor_registry::put(actor_id key, strong_actor_ptr val) {
  CAF_LOG_TRACE(CAF_ARG(key));
  if (! val)
    return;
  { // lifetime scope of guard
    exclusive_guard guard(instances_mtx_);
    if (! entries_.emplace(key, val).second)
      return;
  }
  // attach functor without lock
  CAF_LOG_INFO("added actor:" << CAF_ARG(key));
  actor_registry* reg = this;
  val->get()->attach_functor([key, reg]() {
    reg->erase(key);
  });
}

void actor_registry::erase(actor_id key) {
  exclusive_guard guard(instances_mtx_);
  entries_.erase(key);
}

void actor_registry::inc_running() {
# if defined(CAF_LOG_LEVEL) && CAF_LOG_LEVEL >= CAF_DEBUG
  auto value = ++running_;
  CAF_LOG_DEBUG(CAF_ARG(value));
# else
  ++running_;
# endif
}

size_t actor_registry::running() const {
  return running_.load();
}

void actor_registry::dec_running() {
  size_t new_val = --running_;
  if (new_val <= 1) {
    std::unique_lock<std::mutex> guard(running_mtx_);
    running_cv_.notify_all();
  }
  CAF_LOG_DEBUG(CAF_ARG(new_val));
}

void actor_registry::await_running_count_equal(size_t expected) const {
  CAF_ASSERT(expected == 0 || expected == 1);
  CAF_LOG_TRACE(CAF_ARG(expected));
  std::unique_lock<std::mutex> guard{running_mtx_};
  while (running_ != expected) {
    CAF_LOG_DEBUG(CAF_ARG(running_.load()));
    running_cv_.wait(guard);
  }
}

strong_actor_ptr actor_registry::get(atom_value key) const {
  shared_guard guard{named_entries_mtx_};
  auto i = named_entries_.find(key);
  if (i == named_entries_.end())
    return nullptr;
  return i->second;
}

void actor_registry::put(atom_value key, strong_actor_ptr value) {
  if (value)
    value->get()->attach_functor([=] {
      system_.registry().put(key, nullptr);
    });
  exclusive_guard guard{named_entries_mtx_};
  named_entries_.emplace(key, std::move(value));
}

auto actor_registry::named_actors() const -> name_map {
  shared_guard guard{named_entries_mtx_};
  return named_entries_;
}

namespace {
struct kvstate {
  using key_type = std::string;
  using mapped_type = message;
  using subscriber_set = std::unordered_set<strong_actor_ptr>;
  using topic_set = std::unordered_set<std::string>;
  std::unordered_map<key_type, std::pair<mapped_type, subscriber_set>> data;
  std::unordered_map<strong_actor_ptr, topic_set> subscribers;
  const char* name = "caf.config_server";
  template <class Processor>
  friend void serialize(Processor& proc, kvstate& x, const unsigned int) {
    proc & x.data;
    proc & x.subscribers;
  }
};
} // namespace <anonymous>

void actor_registry::start() {
  auto kvstore = [](stateful_actor<kvstate>* self) -> behavior {
    CAF_LOG_TRACE("");
    std::string wildcard = "*";
    auto unsubscribe_all = [=](actor subscriber) {
      auto& subscribers = self->state.subscribers;
      auto ptr = actor_cast<strong_actor_ptr>(subscriber);
      auto i = subscribers.find(ptr);
      if (i == subscribers.end())
        return;
      for (auto& key : i->second)
        self->state.data[key].second.erase(ptr);
      subscribers.erase(i);
    };
    self->set_down_handler([=](down_msg& dm) {
      CAF_LOG_TRACE(CAF_ARG(dm));
      auto ptr = actor_cast<strong_actor_ptr>(dm.source);
      if (ptr)
        unsubscribe_all(actor_cast<actor>(std::move(ptr)));
    });
    return {
      [=](put_atom, const std::string& key, message& msg) {
        CAF_LOG_TRACE(CAF_ARG(key) << CAF_ARG(msg));
        if (key == "*")
          return;
        auto& vp = self->state.data[key];
        vp.first = std::move(msg);
        for (auto& subscriber_ptr : vp.second) {
          // we never put a nullptr in our map
          auto subscriber = actor_cast<actor>(subscriber_ptr);
          if (subscriber != self->current_sender())
            self->send(subscriber, update_atom::value, key, vp.second);
        }
        // also iterate all subscribers for '*'
        for (auto& subscriber : self->state.data[wildcard].second)
          if (subscriber != self->current_sender())
            self->send(actor_cast<actor>(subscriber), update_atom::value,
                       key, vp.second);
      },
      [=](get_atom, std::string& key) -> message {
        CAF_LOG_TRACE(CAF_ARG(key));
        if (key == wildcard) {
          std::vector<std::pair<std::string, message>> msgs;
          for (auto& kvp : self->state.data)
            if (kvp.first != "*")
              msgs.emplace_back(kvp.first, kvp.second.first);
          return make_message(ok_atom::value, std::move(msgs));
        }
        auto i = self->state.data.find(key);
        return make_message(ok_atom::value, std::move(key),
                            i != self->state.data.end() ? i->second.first
                                                        : make_message());
      },
      [=](subscribe_atom, const std::string& key) {
        auto subscriber = actor_cast<strong_actor_ptr>(self->current_sender());
        CAF_LOG_TRACE(CAF_ARG(key) << CAF_ARG(subscriber));
        if (! subscriber)
          return;
        self->state.data[key].second.insert(subscriber);
        auto& subscribers = self->state.subscribers;
        auto i = subscribers.find(subscriber);
        if (i != subscribers.end()) {
          i->second.insert(key);
        } else {
          self->monitor(subscriber);
          subscribers.emplace(subscriber, kvstate::topic_set{key});
        }
      },
      [=](unsubscribe_atom, const std::string& key) {
        auto subscriber = actor_cast<strong_actor_ptr>(self->current_sender());
        if (! subscriber)
          return;
        CAF_LOG_TRACE(CAF_ARG(key) << CAF_ARG(subscriber));
        if (key == wildcard) {
          unsubscribe_all(actor_cast<actor>(std::move(subscriber)));
          return;
        }
        self->state.subscribers[subscriber].erase(key);
        self->state.data[key].second.erase(subscriber);
      }
    };
  };
  auto spawn_serv = [](event_based_actor* self) -> behavior {
    CAF_LOG_TRACE("");
    return {
      [=](get_atom, const std::string& name, message& args)
      -> result<ok_atom, strong_actor_ptr, std::set<std::string>> {
        CAF_LOG_TRACE(CAF_ARG(name) << CAF_ARG(args));
        actor_config cfg{self->context()};
        auto res = self->system().types().make_actor(name, cfg, args);
        if (! res.first)
          return sec::cannot_spawn_actor_from_arguments;
        return {ok_atom::value, res.first, res.second};
      }
    };
  };
  // NOTE: all actors that we spawn here *must not* access the scheduler,
  //       because the scheduler will make sure that the registry is running
  //       as part of its initialization; hence, all actors have to
  //       use the lazy_init flag
  //named_entries_.emplace(atom("SpawnServ"), system_.spawn_announce_actor_type_server());
  auto cs = system_.spawn<hidden+lazy_init>(kvstore);
  put(atom("ConfigServ"), actor_cast<strong_actor_ptr>(std::move(cs)));
  auto ss = system_.spawn<hidden+lazy_init>(spawn_serv);
  put(atom("SpawnServ"), actor_cast<strong_actor_ptr>(std::move(ss)));
}

void actor_registry::stop() {
  class dropping_execution_unit : public execution_unit {
  public:
    dropping_execution_unit(actor_system* sys) : execution_unit(sys) {
      // nop
    }
    void exec_later(resumable*) override {
      // should not happen in the first place
      CAF_LOG_ERROR("actor registry actor called exec_later during shutdown");
    }
  };
  // the scheduler is already stopped -> invoke exit messages manually
  dropping_execution_unit dummy{&system_};
  for (auto& kvp : named_entries_) {
    auto mp = mailbox_element::make(nullptr, invalid_message_id, {},
                                    exit_msg{kvp.second->address(),
                                             exit_reason::kill});
    auto ptr = static_cast<local_actor*>(actor_cast<abstract_actor*>(kvp.second));
    ptr->exec_single_event(&dummy, mp);
  }
  named_entries_.clear();
}

} // namespace caf
