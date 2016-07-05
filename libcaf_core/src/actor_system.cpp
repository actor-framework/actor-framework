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

#include "caf/actor_system.hpp"

#include <unordered_set>

#include "caf/send.hpp"
#include "caf/to_string.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/actor_system_config.hpp"

#include "caf/policy/work_sharing.hpp"
#include "caf/policy/work_stealing.hpp"

#include "caf/scheduler/coordinator.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"
#include "caf/scheduler/profiled_coordinator.hpp"

namespace caf {

namespace {

struct kvstate {
  using key_type = std::string;
  using mapped_type = message;
  using subscriber_set = std::unordered_set<strong_actor_ptr>;
  using topic_set = std::unordered_set<std::string>;
  std::unordered_map<key_type, std::pair<mapped_type, subscriber_set>> data;
  std::unordered_map<strong_actor_ptr, topic_set> subscribers;
  static const char* name;
  template <class Processor>
  friend void serialize(Processor& proc, kvstate& x, const unsigned int) {
    proc & x.data;
    proc & x.subscribers;
  }
};

const char* kvstate::name = "caf.config_server";

behavior config_serv_impl(stateful_actor<kvstate>* self) {
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
    // set a key/value pair
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
    // get a key/value pair
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
    // subscribe to a key
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
    // unsubscribe from a key
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
    },
    // get a 'named' actor from local registry
    [=](get_atom, atom_value name) {
      return self->home_system().registry().get(name);
    }
  };
}

behavior spawn_serv_impl(event_based_actor* self) {
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
}

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

} // namespace <anonymous>

actor_system::module::~module() {
  // nop
}

actor_system::actor_system(actor_system_config& cfg)
    : ids_(0),
      types_(*this),
      logger_(*this),
      registry_(*this),
      groups_(*this),
      middleman_(nullptr),
      dummy_execution_unit_(this),
      await_actors_before_shutdown_(true),
      detached(0),
      cfg_(cfg) {
  CAF_SET_LOGGER_SYS(this);
  for (auto& f : cfg.module_factories) {
    auto mod_ptr = f(*this);
    modules_[mod_ptr->id()].reset(mod_ptr);
  }
  auto& mmptr = modules_[module::middleman];
  if (mmptr)
    middleman_ = reinterpret_cast<io::middleman*>(mmptr->subtype_ptr());
  auto& clptr = modules_[module::opencl_manager];
  if (clptr)
    opencl_manager_ = reinterpret_cast<opencl::manager*>(clptr->subtype_ptr());
  auto& sched = modules_[module::scheduler];
  using share = scheduler::coordinator<policy::work_sharing>;
  using steal = scheduler::coordinator<policy::work_stealing>;
  using profiled_share = scheduler::profiled_coordinator<policy::work_sharing>;
  using profiled_steal = scheduler::profiled_coordinator<policy::work_stealing>;
  // set scheduler only if not explicitly loaded by user
  if (! sched) {
    enum sched_conf {
      stealing          = 0x0001,
      sharing           = 0x0002,
      profiled          = 0x0100,
      profiled_stealing = 0x0101,
      profiled_sharing  = 0x0102
    };
    sched_conf sc = stealing;
    if (cfg.scheduler_policy == atom("sharing"))
      sc = sharing;
    else if (cfg.scheduler_policy != atom("stealing"))
      std::cerr << "[WARNING] " << deep_to_string(cfg.scheduler_policy)
                << " is an unrecognized scheduler pollicy, "
                   "falling back to 'stealing' (i.e. work-stealing)"
                << std::endl;
    if (cfg.scheduler_enable_profiling)
      sc = static_cast<sched_conf>(sc | profiled);
    switch (sc) {
      default: // any invalid configuration falls back to work stealing
        sched.reset(new steal(*this));
        break;
      case sharing:
        sched.reset(new share(*this));
        break;
      case profiled_stealing:
        sched.reset(new profiled_steal(*this));
        break;
      case profiled_sharing:
        sched.reset(new profiled_share(*this));
        break;
    }
  }
  // initialize state for each module and give each module the opportunity
  // to influence the system configuration, e.g., by adding more types
  for (auto& mod : modules_)
    if (mod)
      mod->init(cfg);
  groups_.init(cfg);
  // spawn config and spawn servers (lazily to not access the scheduler yet)
  static constexpr auto Flags = hidden + lazy_init;
  spawn_serv_ = actor_cast<strong_actor_ptr>(spawn<Flags>(spawn_serv_impl));
  config_serv_ = actor_cast<strong_actor_ptr>(spawn<Flags>(config_serv_impl));
  // fire up remaining modules
  logger_.start();
  registry_.start();
  registry_.put(atom("SpawnServ"), spawn_serv_);
  registry_.put(atom("ConfigServ"), config_serv_);
  for (auto& mod : modules_)
    if (mod)
      mod->start();
  groups_.start();
}

actor_system::~actor_system() {
  if (await_actors_before_shutdown_)
    await_all_actors_done();
  // shutdown system-level servers
  anon_send_exit(spawn_serv_, exit_reason::user_shutdown);
  anon_send_exit(config_serv_, exit_reason::user_shutdown);
  // release memory as soon as possible
  spawn_serv_ = nullptr;
  config_serv_ = nullptr;
  registry_.erase(atom("SpawnServ"));
  registry_.erase(atom("ConfigServ"));
  // group module is the first one, relies on MM
  groups_.stop();
  // stop modules in reverse order
  for (auto i = modules_.rbegin(); i != modules_.rend(); ++i)
    if (*i)
      (*i)->stop();
  await_detached_threads();
  registry_.stop();
  logger_.stop();
  CAF_SET_LOGGER_SYS(nullptr);
}

/// Returns the host-local identifier for this system.
const node_id& actor_system::node() const {
  return node_;
}

/// Returns the scheduler instance.
scheduler::abstract_coordinator& actor_system::scheduler() {
  using ptr = scheduler::abstract_coordinator*;
  return *static_cast<ptr>(modules_[module::scheduler].get());
}

caf::logger& actor_system::logger() {
  return logger_;
}

actor_registry& actor_system::registry() {
  return registry_;
}

const uniform_type_info_map& actor_system::types() const {
  return types_;
}

std::string actor_system::render(const error& x) const {
  auto f = types().renderer(x.category());
  return f ? f(x.code(), x.category(), x.context()) : to_string(x);
}

group_manager& actor_system::groups() {
  return groups_;
}

bool actor_system::has_middleman() const {
  return middleman_ != nullptr;
}

io::middleman& actor_system::middleman() {
  if (! middleman_)
    CAF_RAISE_ERROR("cannot access middleman: module not loaded");
  return *middleman_;
}

bool actor_system::has_opencl_manager() const {
  return opencl_manager_ != nullptr;
}

opencl::manager& actor_system::opencl_manager() const {
  if (! opencl_manager_)
    CAF_RAISE_ERROR("cannot access opencl manager: module not loaded");
  return *opencl_manager_;
}

scoped_execution_unit* actor_system::dummy_execution_unit() {
  return &dummy_execution_unit_;
}

actor_id actor_system::next_actor_id() {
  return ++ids_;
}

actor_id actor_system::latest_actor_id() const {
  return ids_.load();
}

void actor_system::await_all_actors_done() const {
  registry_.await_running_count_equal(0);
}

void actor_system::inc_detached_threads() {
  ++detached;
}

void actor_system::dec_detached_threads() {
  if (--detached == 0) {
    std::unique_lock<std::mutex> guard{detached_mtx};
    detached_cv.notify_all();
  }
}

void actor_system::await_detached_threads() {
  std::unique_lock<std::mutex> guard{detached_mtx};
  while (detached != 0)
    detached_cv.wait(guard);
}

} // namespace caf
