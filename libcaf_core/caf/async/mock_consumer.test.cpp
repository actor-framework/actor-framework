// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/mock_consumer.test.hpp"

namespace caf::async {

void mock_consumer::on_producer_ready() {
  // nop
}

void mock_consumer::on_producer_wakeup() {
  ++wakeups;
}

void mock_consumer::ref_consumer() const noexcept {
  ref();
}

void mock_consumer::deref_consumer() const noexcept {
  deref();
}

} // namespace caf::async
