// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/scoped_coordinator.hpp"

#include "caf/detail/assert.hpp"

namespace caf::flow {

// -- factories ----------------------------------------------------------------

intrusive_ptr<scoped_coordinator> scoped_coordinator::make() {
  return {new scoped_coordinator, false};
}

// -- execution ----------------------------------------------------------------

void scoped_coordinator::run() {
  for (;;) {
    drop_disposed_flows();
    auto f = next(!watched_disposables_.empty());
    if (!f) {
      released_.clear();
      return;
    }
    f.run();
    released_.clear();
  }
}

size_t scoped_coordinator::run_some() {
  size_t result = 0;
  for (;;) {
    drop_disposed_flows();
    auto f = next(false);
    if (!f) {
      released_.clear();
      return result;
    }
    ++result;
    f.run();
    released_.clear();
  }
}

size_t scoped_coordinator::run_some(steady_time_point timeout) {
  size_t result = 0;
  for (;;) {
    drop_disposed_flows();
    auto f = next(timeout);
    if (!f) {
      released_.clear();
      return result;
    }
    ++result;
    f.run();
    released_.clear();
  }
}

// -- reference counting -------------------------------------------------------

void scoped_coordinator::ref_execution_context() const noexcept {
  ref();
}

void scoped_coordinator::deref_execution_context() const noexcept {
  deref();
}

// -- properties ---------------------------------------------------------------

size_t scoped_coordinator::pending_actions() const noexcept {
  std::unique_lock guard{mtx_};
  return actions_.size() + delayed_.size();
}

// -- lifetime management ------------------------------------------------------

void scoped_coordinator::release_later(coordinated_ptr& child) {
  CAF_ASSERT(child != nullptr);
  released_.emplace_back().swap(child);
}

void scoped_coordinator::watch(disposable what) {
  watched_disposables_.emplace_back(std::move(what));
}

// -- time ---------------------------------------------------------------------

coordinator::steady_time_point scoped_coordinator::steady_time() {
  return std::chrono::steady_clock::now();
}

// -- scheduling of actions ----------------------------------------------------

void scoped_coordinator::schedule(action what) {
  std::unique_lock guard{mtx_};
  actions_.emplace_back(std::move(what));
  if (actions_.size() == 1)
    cv_.notify_all();
}

void scoped_coordinator::delay(action what) {
  schedule(std::move(what));
}

disposable scoped_coordinator::delay_until(steady_time_point abs_time,
                                           action what) {
  using namespace std::literals;
  delayed_.emplace(abs_time, what);
  return what.as_disposable();
}

void scoped_coordinator::drop_disposed_flows() {
  auto disposed = [](auto& hdl) { return hdl.disposed(); };
  auto& xs = watched_disposables_;
  if (auto e = std::remove_if(xs.begin(), xs.end(), disposed); e != xs.end())
    xs.erase(e, xs.end());
}

// -- queue and schedule access ------------------------------------------------

action scoped_coordinator::next(bool blocking) {
  if (!delayed_.empty()) {
    auto n = std::chrono::steady_clock::now();
    auto i = delayed_.begin();
    if (n >= i->first) {
      auto res = std::move(i->second);
      delayed_.erase(i);
      return res;
    }
    auto tout = i->first;
    std::unique_lock guard{mtx_};
    while (actions_.empty()) {
      if (cv_.wait_until(guard, tout) == std::cv_status::timeout) {
        auto res = std::move(i->second);
        delayed_.erase(i);
        return res;
      }
    }
    auto res = std::move(actions_[0]);
    actions_.erase(actions_.begin());
    return res;
  }
  std::unique_lock guard{mtx_};
  if (blocking) {
    while (actions_.empty())
      cv_.wait(guard);
  } else if (actions_.empty()) {
    return action{};
  }
  auto res = std::move(actions_[0]);
  actions_.erase(actions_.begin());
  return res;
}

action scoped_coordinator::next(steady_time_point timeout) {
  // Dispatch to the regular blocking version if we have an action that becomes
  // due before the timeout.
  if (!delayed_.empty() && delayed_.begin()->first <= timeout)
    return next(true);
  // Short-circuit if we have no watched disposables, i.e., have no dependencies
  // on external events.
  if (watched_disposables_.empty())
    return next(false);
  // Otherwise, wait on the condition variable if necessary.
  std::unique_lock guard{mtx_};
  while (actions_.empty()) {
    if (cv_.wait_until(guard, timeout) == std::cv_status::timeout)
      return action{};
  }
  auto res = std::move(actions_.front());
  actions_.pop_front();
  return res;
}

} // namespace caf::flow
