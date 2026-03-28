// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_ref_counted.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/intrusive_ptr.hpp"

namespace caf::async {

/// Base type for asynchronous consumers of an event source.
class CAF_CORE_EXPORT consumer : public abstract_ref_counted {
public:
  virtual ~consumer();

  /// Called to signal that the producer started emitting items.
  virtual void on_producer_ready() = 0;

  /// Called to signal to the consumer that the producer added an item to a
  /// previously empty source or completed the flow of events.
  virtual void on_producer_wakeup() = 0;
};

/// @relates consumer
using consumer_ptr = intrusive_ptr<consumer>;

} // namespace caf::async
