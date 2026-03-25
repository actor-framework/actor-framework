// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/async/consumer.hpp"
#include "caf/detail/atomic_ref_count.hpp"

#include <atomic>

namespace caf::async {

class mock_consumer : public consumer {
public:
  void on_producer_ready() override;

  void on_producer_wakeup() override;

  void ref_consumer() const noexcept override;

  void deref_consumer() const noexcept override;

  mutable detail::atomic_ref_count ref_count_;

  /// Incremented whenever `on_producer_wakeup` is called.
  std::atomic<size_t> wakeups = 0;
};

} // namespace caf::async
