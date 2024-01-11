// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/after.hpp"
#include "caf/defaults.hpp"
#include "caf/logger.hpp"
#include "caf/others.hpp"
#include "caf/policy/work_stealing.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/scheduler/coordinator.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/send.hpp"
#include "caf/system_messages.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>

namespace caf::scheduler {

// -- utility and implementation details ---------------------------------------

namespace {

class actor_local_printer_impl : public detail::actor_local_printer {
public:
  actor_local_printer_impl(abstract_actor* self, actor printer)
    : self_(self->id()), printer_(std::move(printer)) {
    CAF_ASSERT(printer_ != nullptr);
    if (!self->getf(abstract_actor::has_used_aout_flag))
      self->setf(abstract_actor::has_used_aout_flag);
  }

  void write(std::string&& arg) override {
    printer_->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           add_atom_v, self_, std::move(arg)),
                      nullptr);
  }

  void write(const char* arg) override {
    write(std::string{arg});
  }

  void flush() override {
    printer_->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           flush_atom_v, self_),
                      nullptr);
  }

private:
  actor_id self_;
  actor printer_;
};

using string_sink = std::function<void(std::string&&)>;

using string_sink_ptr = std::shared_ptr<string_sink>;

using sink_cache = std::map<std::string, string_sink_ptr>;

string_sink make_sink(actor_system&, const std::string& fn, int flags) {
  if (fn.empty()) {
    return nullptr;
  } else if (fn.front() == ':') {
    // TODO: re-implement "virtual files" or remove
    return nullptr;
  } else {
    auto append = (flags & actor_ostream::append) != 0;
    auto fs = std::make_shared<std::ofstream>();
    fs->open(fn, append ? std::ios_base::out | std::ios_base::app
                        : std::ios_base::out);
    if (fs->is_open()) {
      return [fs](std::string&& out) { *fs << out; };
    } else {
      std::cerr << "cannot open file: " << fn << std::endl;
      return nullptr;
    }
  }
}

string_sink_ptr get_or_add_sink_ptr(actor_system& sys, sink_cache& fc,
                                    const std::string& fn, int flags) {
  if (auto i = fc.find(fn); i != fc.end()) {
    return i->second;
  } else if (auto fs = make_sink(sys, fn, flags)) {
    if (fs) {
      auto ptr = std::make_shared<string_sink>(std::move(fs));
      fc.emplace(fn, ptr);
      return ptr;
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

class printer_actor : public blocking_actor {
public:
  printer_actor(actor_config& cfg) : blocking_actor(cfg) {
    // nop
  }

  void act() override {
    struct actor_data {
      std::string current_line;
      string_sink_ptr redirect;
      actor_data() {
        // nop
      }
    };
    using data_map = std::unordered_map<actor_id, actor_data>;
    sink_cache fcache;
    string_sink_ptr global_redirect;
    data_map data;
    auto get_data = [&](actor_id addr, bool insert_missing) -> actor_data* {
      if (addr == invalid_actor_id)
        return nullptr;
      auto i = data.find(addr);
      if (i == data.end() && insert_missing)
        i = data.emplace(addr, actor_data{}).first;
      if (i != data.end())
        return &(i->second);
      return nullptr;
    };
    auto flush = [&](actor_data* what, bool forced) {
      if (what == nullptr)
        return;
      auto& line = what->current_line;
      if (line.empty() || (line.back() != '\n' && !forced))
        return;
      if (what->redirect)
        (*what->redirect)(std::move(line));
      else if (global_redirect)
        (*global_redirect)(std::move(line));
      else
        std::cout << line << std::flush;
      line.clear();
    };
    bool done = false;
    do_receive(
      [&](add_atom, actor_id aid, std::string& str) {
        if (str.empty() || aid == invalid_actor_id)
          return;
        auto d = get_data(aid, true);
        if (d != nullptr) {
          d->current_line += str;
          flush(d, false);
        }
      },
      [&](flush_atom, actor_id aid) { flush(get_data(aid, false), true); },
      [&](delete_atom, actor_id aid) {
        auto data_ptr = get_data(aid, false);
        if (data_ptr != nullptr) {
          flush(data_ptr, true);
          data.erase(aid);
        }
      },
      [&](redirect_atom, const std::string& fn, int flag) {
        global_redirect = get_or_add_sink_ptr(system(), fcache, fn, flag);
      },
      [&](redirect_atom, actor_id aid, const std::string& fn, int flag) {
        auto d = get_data(aid, true);
        if (d != nullptr)
          d->redirect = get_or_add_sink_ptr(system(), fcache, fn, flag);
      },
      [&](exit_msg& em) {
        fail_state(std::move(em.reason));
        done = true;
      })
      .until([&] { return done; });
  }

  const char* name() const override {
    return "printer_actor";
  }
};

} // namespace

// -- implementation of coordinator --------------------------------------------

detail::actor_local_printer_ptr
abstract_coordinator::printer_for(local_actor* self) {
  return make_counted<actor_local_printer_impl>(self, printer());
}

const actor_system_config& abstract_coordinator::config() const {
  return system_.config();
}

bool abstract_coordinator::detaches_utility_actors() const {
  return true;
}

void abstract_coordinator::start() {
  CAF_LOG_TRACE("");
  // launch utility actors
  static constexpr auto fs = hidden + detached;
  utility_actors_[printer_id] = system_.spawn<printer_actor, fs>();
}

void abstract_coordinator::init(actor_system_config& cfg) {
  namespace sr = defaults::scheduler;
  max_throughput_ = get_or(cfg, "caf.scheduler.max-throughput",
                           sr::max_throughput);
  num_workers_ = get_or(cfg, "caf.scheduler.max-threads",
                        default_thread_count());
}

actor_system::module::id_t abstract_coordinator::id() const {
  return module::scheduler;
}

void* abstract_coordinator::subtype_ptr() {
  return this;
}

void abstract_coordinator::stop_actors() {
  CAF_LOG_TRACE("");
  scoped_actor self{system_, true};
  for (auto& x : utility_actors_)
    anon_send_exit(x, exit_reason::user_shutdown);
  self->wait_for(utility_actors_);
}

abstract_coordinator::abstract_coordinator(actor_system& sys)
  : next_worker_(0), max_throughput_(0), num_workers_(0), system_(sys) {
  // nop
}

void abstract_coordinator::cleanup_and_release(resumable* ptr) {
  class dummy_unit : public execution_unit {
  public:
    dummy_unit(local_actor* job) : execution_unit(&job->home_system()) {
      // nop
    }
    void exec_later(resumable* job) override {
      resumables.push_back(job);
    }
    std::vector<resumable*> resumables;
  };
  switch (ptr->subtype()) {
    case resumable::scheduled_actor:
    case resumable::io_actor: {
      auto dptr = static_cast<scheduled_actor*>(ptr);
      dummy_unit dummy{dptr};
      dptr->cleanup(make_error(exit_reason::user_shutdown), &dummy);
      while (!dummy.resumables.empty()) {
        auto sub = dummy.resumables.back();
        dummy.resumables.pop_back();
        switch (sub->subtype()) {
          case resumable::scheduled_actor:
          case resumable::io_actor: {
            auto dsub = static_cast<scheduled_actor*>(sub);
            dsub->cleanup(make_error(exit_reason::user_shutdown), &dummy);
            break;
          }
          default:
            break;
        }
      }
      break;
    }
    default:
      break;
  }
  intrusive_ptr_release(ptr);
}

size_t abstract_coordinator::default_thread_count() noexcept {
  return std::max(std::thread::hardware_concurrency(), 4u);
}

} // namespace caf::scheduler
