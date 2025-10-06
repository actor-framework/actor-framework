// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/flow/op/auto_connect.hpp"

namespace caf::flow::op {

/// Turns a connectable into an observable that automatically connects to the
/// source when reaching the subscriber threshold and disconnects automatically
/// after the last subscriber canceled its subscription. After a disconnect, the
/// operator reconnects to the source again if a new subscriber appears (the
/// threshold only applies to the initial connect).
template <class T>
class ref_count : public auto_connect<T> {
public:
  using super = auto_connect<T>;

  ref_count(coordinator* parent, size_t threshold,
            intrusive_ptr<connectable<T>> source)
    : super(parent, threshold, source) {
    super::state_->auto_disconnect = true;
  }
};

} // namespace caf::flow::op
