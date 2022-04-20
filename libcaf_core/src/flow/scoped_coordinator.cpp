// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/scoped_coordinator.hpp"

namespace caf::flow {

// -- factories ----------------------------------------------------------------

intrusive_ptr<scoped_coordinator> scoped_coordinator::make() {
  return {new scoped_coordinator, false};
}

// -- execution ----------------------------------------------------------------

void scoped_coordinator::run() {
  for (;;) {
    auto f = next(!watched_disposables_.empty());
    if (f.ptr() != nullptr) {
      f.run();
      drop_disposed_flows();
    } else {
      return;
    }
  }
}

// -- reference counting -------------------------------------------------------

void scoped_coordinator::ref_coordinator() const noexcept {
  ref();
}

void scoped_coordinator::deref_coordinator() const noexcept {
  deref();
}

// -- lifetime management ------------------------------------------------------

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
  } else {
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
}

} // namespace caf::flow
