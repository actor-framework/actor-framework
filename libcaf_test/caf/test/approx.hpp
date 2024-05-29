// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <limits>

namespace caf::test {

/// Allows comparing a value of type `T` with a configurable precision.
template <class T>
class approx {
public:
  explicit approx(T value) : value_(value) {
  }

  approx epsilon(T eps) const {
    auto copy = *this;
    copy.epsilon_ = eps;
    return copy;
  }

  friend bool operator==(T lhs, approx<T> rhs) {
    return lhs >= rhs.min_value() && lhs <= rhs.max_value();
  }

private:
  T min_value() {
    return value_ - epsilon_;
  }

  T max_value() {
    return value_ + epsilon_;
  }

  T value_;
  T epsilon_ = std::numeric_limits<T>::epsilon();
};

} // namespace caf::test
