// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/daemons.hpp"

#include "caf/anon_mail.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/send.hpp"

#include <cstdint>
#include <map>
#include <mutex>
#include <thread>

namespace caf::detail {

struct daemons::impl {
  explicit impl(actor_system& sys) : sys(&sys) {
    // nop
  }

  void on_start() {
    cleaner = sys->spawn<hidden>([this](caf::event_based_actor* self) {
      return behavior{
        [this, self](actor hdl, int64_t id) {
          self->monitor(hdl, [this, id](const error&) { cleanup(id); });
        },
      };
    });
  }

  void on_stop() {
    anon_send_exit(cleaner, exit_reason::user_shutdown);
    std::map<int64_t, impl::state> stopped_workers;
    {
      std::lock_guard guard{mtx};
      id = 0;
      stopped_workers.swap(workers);
    }
    for (auto& [id, st] : stopped_workers) {
      st.do_stop(std::move(st.hdl));
    }
  }

  struct state {
    actor hdl;
    std::function<void(actor)> do_stop;
  };

  void cleanup(int64_t id) {
    std::lock_guard guard{mtx};
    if (auto i = workers.find(id); i != workers.end()) {
      workers.erase(i);
    }
  }

  actor_system* sys;

  /// Monitors background threads and cleans up their state.
  actor cleaner;

  /// Protects other member variables.
  std::mutex mtx;

  /// Next ID to assign to a background thread or 0 if the module is stopped.
  int64_t id = 1;

  /// Maps daemon IDs to their state.
  std::map<int64_t, state> workers;
};

daemons::daemons(actor_system& sys) : impl_(new impl(sys)) {
  // nop
}

daemons::~daemons() {
  delete impl_;
}

actor daemons::do_launch(std::function<actor(actor_system&)> do_spawn,
                         std::function<void(actor)> do_stop) {
  std::lock_guard guard{impl_->mtx};
  if (impl_->id == 0) {
    return {};
  }
  auto id = impl_->id++;
  auto& st = impl_->workers[id];
  st.hdl = do_spawn(*impl_->sys);
  st.do_stop = std::move(do_stop);
  anon_mail(st.hdl, id).send(impl_->cleaner);
  return st.hdl;
}

void daemons::start() {
  impl_->on_start();
}

void daemons::stop() {
  impl_->on_stop();
}

void daemons::init(caf::actor_system_config&) {
  // nop
}

caf::actor_system_module::id_t daemons::id() const {
  return id_t::daemons;
}

void* daemons::subtype_ptr() {
  return this;
}

} // namespace caf::detail
