// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail {

template <template <class, typename> class Tpl, typename Arg1>
struct tbind {
  template <class Arg2>
  struct type {
    static constexpr bool value = Tpl<Arg1, Arg2>::value;
  };
};

} // namespace caf::detail
