// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail {

template <class Processor, class T>
auto try_serialize(Processor& proc, T* x) -> decltype(proc & *x) {
  proc&* x;
}

template <class Processor>
void try_serialize(Processor&, void*) {
  // nop
}

} // namespace caf::detail
