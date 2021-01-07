// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf {

template <class F>
struct timeout_definition;

class behavior;

template <class T>
struct may_have_timeout {
  static constexpr bool value = false;
};

template <>
struct may_have_timeout<behavior> {
  static constexpr bool value = true;
};

template <class F>
struct may_have_timeout<timeout_definition<F>> {
  static constexpr bool value = true;
};

} // namespace caf
