// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/daemons.hpp"

#include <cstdint>
#include <map>
#include <mutex>
#include <thread>

namespace caf::detail {

struct daemons::impl {
  int64_t next_id() {
    std::lock_guard guard{mtx};
    if (id == 0)
      return 0;
    return id++;
  }

  struct state {
    std::thread t;
    std::function<void()> on_stop;
  };

  /// Protects other member variables.
  mutable std::mutex mtx;

  /// Next ID to assign to a background thread or 0 if the module is stopped.
  int64_t id = 1;

  /// Maps daemon IDs to their state.
  std::map<int64_t, state> threads;
};

daemons::daemons() : impl_(new impl) {
  // nop
}

daemons::~daemons() {
  delete impl_;
}

void daemons::cleanup(int id) {
  std::lock_guard guard{impl_->mtx};
  auto i = impl_->threads.find(id);
  auto& [t, on_stop] = i->second;
  on_stop();
  t.join();
  impl_->threads.erase(i);
}

bool daemons::launch(std::function<void()> fn, std::function<void()> stop) {
  auto id = impl_->next_id();
  if (id == 0) {
    return false;
  }
  auto t = std::thread{[fn = std::move(fn)] { fn(); }};
  std::lock_guard guard{impl_->mtx};
  auto& st = impl_->threads[id];
  st.t = std::move(t);
  st.on_stop = std::move(stop);
  return true;
}

void daemons::start() {
  // nop
}

void daemons::stop() {
  std::map<int64_t, impl::state> threads;
  {
    std::lock_guard guard{impl_->mtx};
    impl_->id = 0;
    impl_->threads.swap(threads);
  }
  for (auto& [id, st] : threads) {
    st.on_stop();
    st.t.join();
  }
}

void daemons::init(caf::actor_system_config&) {
  // nop
}

caf::actor_system_module::id_t daemons::id() const {
  return id_t::daemons;
}

void* daemons::subtype_ptr() {
  return static_cast<caf::actor_system_module*>(this);
}

} // namespace caf::detail
