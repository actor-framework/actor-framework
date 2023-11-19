// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/flow.hpp"

namespace caf::test::fixture {

namespace {

/// A trivial disposable with an atomic flag.
class trivial_impl : public caf::ref_counted, public caf::disposable::impl {
public:
  trivial_impl() : flag_(false) {
    // nop
  }

  void dispose() override {
    flag_ = true;
  }

  bool disposed() const noexcept override {
    return flag_.load();
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  friend void intrusive_ptr_add_ref(const trivial_impl* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const trivial_impl* ptr) noexcept {
    ptr->deref();
  }

private:
  std::atomic<bool> flag_;
};

} // namespace

caf::disposable flow::make_trivial_disposable() {
  return disposable{make_counted<trivial_impl>()};
}

caf::flow::coordinator*
flow::passive_subscription_impl::parent() const noexcept {
  return parent_;
}

void flow::passive_subscription_impl::request(size_t n) {
  demand += n;
}

void flow::passive_subscription_impl::dispose() {
  disposed_flag = true;
}

bool flow::passive_subscription_impl::disposed() const noexcept {
  return disposed_flag;
}

std::string flow::to_string(observer_state x) {
  switch (x) {
    case observer_state::idle:
      return "idle";
    case observer_state::subscribed:
      return "subscribed";
    case observer_state::completed:
      return "completed";
    case observer_state::aborted:
      return "aborted";
    default:
      return "<invalid>";
  }
}

void flow::run_flows() {
  // TODO: the scoped_coordinator is not the right tool for this job. We need a
  //       custom coordinator that allows us to control timeouts. For now, this
  //       is only good enough to get tests running that have no notion of time.
  coordinator_->run_some();
}

} // namespace caf::test::fixture
