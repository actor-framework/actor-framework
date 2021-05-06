// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <condition_variable>
#include <type_traits>

#include "caf/async/observer_buffer.hpp"

namespace caf::async {

/// Consumes all elements from a publisher and blocks the current thread until
/// completion.
template <class T>
class blocking_observer : public observer_buffer<T> {
public:
  using super = observer_buffer<T>;

  using super::super;

  template <class OnNext, class OnError, class OnComplete>
  auto run(OnNext fun, OnError err, OnComplete fin) {
    static_assert(std::is_invocable_v<OnNext, T>,
                  "OnNext handlers must have signature 'void(T)' or 'bool(T)'");
    static_assert(std::is_invocable_v<OnError, error>,
                  "OnError handlers must have signature 'void(const error&)'");
    static_assert(std::is_invocable_v<OnComplete>,
                  "OnComplete handlers must have signature 'void()'");
    auto wait_fn = [this](std::unique_lock<std::mutex>& guard) {
      cv_.wait(guard);
    };
    for (;;) {
      auto [val, done, what] = super::wait_with(wait_fn);
      if (val) {
        using fun_res = decltype(fun(*val));
        if constexpr (std::is_same_v<bool, fun_res>) {
          if (!fun(*val)) {
            std::unique_lock guard{super::mtx_};
            if (super::sub_)
              super::sub_->cancel();
            return;
          }
        } else {
          static_assert(
            std::is_same_v<void, fun_res>,
            "OnNext handlers must have signature 'void(T)' or 'bool(T)'");
          fun(*val);
        }
      } else if (done) {
        if (!what)
          fin();
        else
          err(*what);
        return;
      }
    }
  }

private:
  std::condition_variable cv_;

  void wakeup(std::unique_lock<std::mutex>&) override {
    cv_.notify_all();
  }
};

} // namespace caf::async
