// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <utility>

namespace caf::detail {

template <class T>
class consumer {
public:
  using value_type = T;

  explicit consumer(T& x) : x_(x) {
    // nop
  }

  void value(T&& y) {
    x_ = std::move(y);
  }

private:
  T& x_;
};

template <class T>
consumer<T> make_consumer(T& x) {
  return consumer<T>{x};
}

} // namespace caf::detail
