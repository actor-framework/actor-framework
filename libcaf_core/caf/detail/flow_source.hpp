// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/fwd.hpp"
#include "caf/ref_counted.hpp"

namespace caf::detail {

template <class T>
class flow_source : public ref_counted {
public:
  virtual ~flow_source() {
    // nop
  }

  virtual void add(async::producer_resource<T> sink) = 0;
};

template <class T>
using flow_source_ptr = intrusive_ptr<flow_source<T>>;

} // namespace caf::detail
