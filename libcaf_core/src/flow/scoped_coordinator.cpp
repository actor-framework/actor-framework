// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/scoped_coordinator.hpp"

namespace caf::flow {

void scoped_coordinator::run() {
  auto next = [this](bool blocking) {
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
  };
  for (;;) {
    auto hdl = next(!watched_disposables_.empty());
    if (hdl.ptr() != nullptr) {
      hdl.run();
      drop_disposed_flows();
    } else {
      return;
    }
  }
}

void scoped_coordinator::ref_coordinator() const noexcept {
  ref();
}

void scoped_coordinator::deref_coordinator() const noexcept {
  deref();
}

void scoped_coordinator::schedule(action what) {
  std::unique_lock guard{mtx_};
  actions_.emplace_back(std::move(what));
  if (actions_.size() == 1)
    cv_.notify_all();
}

void scoped_coordinator::post_internally(action what) {
  schedule(std::move(what));
}

void scoped_coordinator::watch(disposable what) {
  watched_disposables_.emplace_back(std::move(what));
}

intrusive_ptr<scoped_coordinator> scoped_coordinator::make() {
  return {new scoped_coordinator, false};
}

void scoped_coordinator::drop_disposed_flows() {
  auto disposed = [](auto& hdl) { return hdl.disposed(); };
  auto& xs = watched_disposables_;
  if (auto e = std::remove_if(xs.begin(), xs.end(), disposed); e != xs.end())
    xs.erase(e, xs.end());
}

} // namespace caf::flow
