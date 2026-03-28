// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/async/producer.hpp"
#include "caf/detail/atomic_ref_count.hpp"

#include <atomic>

namespace caf::async {

class mock_producer : public producer {
public:
  void on_consumer_ready() override;

  void on_consumer_cancel() override;

  void on_consumer_demand(size_t new_demand) override;

  void ref() const noexcept final;

  void deref() const noexcept final;

  /// Incremented whenever `on_consumer_ready` is called.
  std::atomic<size_t> wakeups = 0;

  /// Incremented whenever `on_consumer_demand` is called.
  std::atomic<size_t> demand = 0;

  /// Set to true if `on_consumer_cancel` is called.
  std::atomic<bool> canceled = false;

private:
  mutable detail::atomic_ref_count ref_count_;
};

} // namespace caf::async
