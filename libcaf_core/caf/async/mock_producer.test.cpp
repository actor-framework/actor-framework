// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/mock_producer.test.hpp"

namespace caf::async {

void mock_producer::on_consumer_ready() {
  ++wakeups;
}

void mock_producer::on_consumer_cancel() {
  canceled = true;
}

void mock_producer::on_consumer_demand(size_t new_demand) {
  demand += new_demand;
}

void mock_producer::ref() const noexcept {
  ref_count_.inc();
}

void mock_producer::deref() const noexcept {
  ref_count_.dec(this);
}

} // namespace caf::async
