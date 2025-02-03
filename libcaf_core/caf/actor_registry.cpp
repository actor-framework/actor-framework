// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_registry.hpp"

#include "caf/actor_system.hpp"
#include "caf/attachable.hpp"
#include "caf/detail/assert.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/exit_reason.hpp"
#include "caf/log/core.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/sec.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/telemetry/metric_family_impl.hpp"

#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace caf {

namespace {

using exclusive_guard = std::unique_lock<std::shared_mutex>;
using shared_guard = std::shared_lock<std::shared_mutex>;

} // namespace

actor_registry::~actor_registry() {
  // nop
}

actor_registry::actor_registry(actor_system& sys) : system_(sys) {
  // nop
}

strong_actor_ptr actor_registry::get_impl(actor_id key) const {
  shared_guard guard(instances_mtx_);
  auto i = entries_.find(key);
  if (i != entries_.end())
    return i->second;
  log::core::debug("key invalid, assume actor no longer exists: key = {}", key);
  return nullptr;
}

void actor_registry::put_impl(actor_id key, strong_actor_ptr val) {
  auto lg = log::core::trace("key = {}", key);
  if (!val)
    return;
  { // lifetime scope of guard
    exclusive_guard guard(instances_mtx_);
    if (!entries_.emplace(key, val).second)
      return;
  }
  // attach functor without lock
  log::core::debug("added actor: key = {}", key);
  actor_registry* reg = this;
  val->get()->attach_functor([key, reg]() { reg->erase(key); });
}

void actor_registry::erase(actor_id key) {
  // Stores a reference to the actor we're going to remove. This guarantees
  // that we aren't releasing the last reference to an actor while erasing it.
  // Releasing the final ref can trigger the actor to call its cleanup function
  // that in turn calls this function and we can end up in a deadlock.
  strong_actor_ptr ref;
  { // Lifetime scope of guard.
    exclusive_guard guard{instances_mtx_};
    auto i = entries_.find(key);
    if (i != entries_.end()) {
      ref.swap(i->second);
      entries_.erase(i);
    }
  }
}

size_t actor_registry::inc_running(std::string_view name) {
  ++*system_.base_metrics().running_actors_by_name->get_or_add({{"name", name}});
  return ++*system_.base_metrics().running_actors;
}

size_t actor_registry::running() const {
  return static_cast<size_t>(system_.base_metrics().running_actors->value());
}

size_t actor_registry::dec_running(std::string_view name) {
  --*system_.base_metrics().running_actors_by_name->get_or_add({{"name", name}});
  size_t new_val = --*system_.base_metrics().running_actors;
  if (new_val <= 1) {
    std::unique_lock<std::mutex> guard(running_mtx_);
    running_cv_.notify_all();
  }
  return new_val;
}

void actor_registry::await_running_count_equal(size_t expected) const {
  CAF_ASSERT(expected == 0 || expected == 1);
  auto lg = log::core::trace("expected = {}", expected);
  std::unique_lock<std::mutex> guard{running_mtx_};
  while (running() != expected) {
    log::core::debug("running = {}", running());
    running_cv_.wait(guard);
  }
}

strong_actor_ptr actor_registry::get_impl(const std::string& key) const {
  shared_guard guard{named_entries_mtx_};
  auto i = named_entries_.find(key);
  if (i == named_entries_.end())
    return nullptr;
  return i->second;
}

void actor_registry::put_impl(const std::string& key, strong_actor_ptr value) {
  if (value == nullptr) {
    erase(key);
    return;
  }
  exclusive_guard guard{named_entries_mtx_};
  named_entries_.emplace(std::move(key), std::move(value));
}

void actor_registry::erase(const std::string& key) {
  // Stores a reference to the actor we're going to remove for the same
  // reasoning as in erase(actor_id).
  strong_actor_ptr ref;
  { // Lifetime scope of guard.
    exclusive_guard guard{named_entries_mtx_};
    auto i = named_entries_.find(key);
    if (i != named_entries_.end()) {
      ref.swap(i->second);
      named_entries_.erase(i);
    }
  }
}

auto actor_registry::named_actors() const -> name_map {
  shared_guard guard{named_entries_mtx_};
  return named_entries_;
}

void actor_registry::start() {
  // nop
}

void actor_registry::stop() {
  {
    exclusive_guard guard{instances_mtx_};
    entries_.clear();
  }
  {
    exclusive_guard guard{named_entries_mtx_};
    named_entries_.clear();
  }
}

} // namespace caf
