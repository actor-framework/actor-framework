// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/stream_bridge.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/anon_mail.hpp"
#include "caf/async/batch.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/make_counted.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/system_messages.hpp"

#include <memory>
#include <vector>

using namespace caf;

namespace {

struct observer_state {
  flow::subscription sub;
  std::vector<int> values;
  error reason;
  size_t errors = 0;
  size_t completions = 0;
  size_t destructions = 0;
  bool cancel_on_next = false;
};

class batch_observer : public flow::observer_impl_base<async::batch> {
public:
  batch_observer(flow::coordinator* parent,
                 std::shared_ptr<observer_state> state)
    : parent_(parent), state_(std::move(state)) {
    // nop
  }

  ~batch_observer() override {
    ++state_->destructions;
  }

  flow::coordinator* parent() const noexcept override {
    return parent_;
  }

  void on_subscribe(flow::subscription sub) override {
    state_->sub = std::move(sub);
  }

  void on_next(const async::batch& input) override {
    if (state_->cancel_on_next)
      state_->sub.cancel();
    // The current batch must remain valid even when cancellation clears buf_.
    auto items = input.items<int>();
    state_->values.insert(state_->values.end(), items.begin(), items.end());
  }

  void on_error(const error& reason) override {
    ++state_->errors;
    state_->reason = reason;
    state_->sub.release_later();
  }

  void on_complete() override {
    ++state_->completions;
    state_->sub.release_later();
  }

private:
  flow::coordinator* parent_;
  std::shared_ptr<observer_state> state_;
};

struct fixture : test::fixture::deterministic {
  std::shared_ptr<observer_state> state = std::make_shared<observer_state>();
  actor src = sys.spawn([] { return behavior{[](int) {}}; });
  actor snk = sys.spawn([](event_based_actor* self) {
    return behavior{[self](int value) {
      if (value == 1)
        self->unbecome();
    }};
  });
  disposable sub;
  uint64_t sink_id = 0;

  fixture() {
    auto* self = actor_cast<scheduled_actor*>(snk);
    auto obs = self->add_child(std::in_place_type<batch_observer>, state);
    auto src_ptr = actor_cast<strong_actor_ptr>(src);
    auto input = make_counted<detail::stream_bridge>(self, src_ptr, 42u, 5u,
                                                     3u);
    sub = input->subscribe(obs->as_observer());
    auto open = stream_open_msg{};
    test::runnable::current().require(
      prepone<stream_open_msg>().with(std::ref(open)).to(src));
    sink_id = open.sink_flow_id;
    anon_mail(stream_ack_msg{src_ptr, sink_id, 7u, 1u}).send(snk);
    expect<stream_ack_msg>().to(snk);
    prepone_and_expect<stream_demand_msg>().from(snk).to(src);
  }

  template <class T>
  void push(T value) {
    anon_mail(stream_batch_msg{sink_id, async::make_batch(std::vector{value})})
      .send(snk);
    expect<stream_batch_msg>().to(snk);
  }

  void run() {
    inject().with(0).to(snk);
  }

  void check_canceled() {
    auto& ctx = test::runnable::current();
    auto cancel = stream_cancel_msg{};
    ctx.require(
      prepone_and_expect<stream_cancel_msg>().with(std::ref(cancel)).to(src));
    ctx.check_eq(cancel.source_flow_id, 7u);
    ctx.check(!prepone<stream_cancel_msg>().to(src));
    ctx.check(!prepone<stream_demand_msg>().to(src));
  }

  void check_released() {
    auto& ctx = test::runnable::current();
    // Keep sub alive so observer destruction cannot depend on destroying it.
    ctx.check(sub.disposed());
    ctx.check_eq(state->destructions, 1u);
    inject().with(1).to(snk);
    ctx.check(terminated(snk));
  }
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("disposing a bridge subscription reports disposed in on_error") {
  push(10);
  push(20);
  auto payload = std::make_shared<int>(42);
  auto weak_payload = std::weak_ptr<int>{payload};
  push(std::move(payload));
  check(!weak_payload.expired());
  check(!sub.disposed());
  sub.ptr()->dispose();
  sub.ptr()->dispose();
  run();
  check(weak_payload.expired());
  check_canceled();
  check_eq(state->errors, 1u);
  check_eq(state->reason, sec::disposed);
  check_eq(state->completions, 0u);
  check(state->values.empty());
  check(!state->sub);
  push(30);
  anon_mail(stream_close_msg{sink_id}).send(snk);
  expect<stream_close_msg>().to(snk);
  anon_mail(stream_abort_msg{sink_id, make_error(sec::runtime_error)})
    .send(snk);
  expect<stream_abort_msg>().to(snk);
  sub.ptr()->dispose();
  run();
  check_eq(state->errors, 1u);
  check_eq(state->completions, 0u);
  check(state->values.empty());
  check_released();
}

TEST("canceling a bridge subscription skips terminal events") {
  push(10);
  auto payload = std::make_shared<int>(42);
  auto weak_payload = std::weak_ptr<int>{payload};
  push(std::move(payload));
  check(!weak_payload.expired());
  auto again = state->sub;
  state->sub.cancel();
  again.cancel();
  sub.ptr()->dispose();
  check(sub.disposed());
  check_eq(state->destructions, 0u);
  run();
  check(weak_payload.expired());
  check_canceled();
  check_eq(state->errors, 0u);
  check_eq(state->completions, 0u);
  check(state->reason.empty());
  check(state->values.empty());
  check_released();
}

TEST("canceling from on_next stops buffered bridge delivery") {
  push(10);
  push(20);
  push(30);
  state->sub.request(1);
  run();
  check_eq(state->values, std::vector{10});
  auto pending = state->sub;
  state->cancel_on_next = true;
  state->sub.request(3);
  run();
  check_canceled();
  check_eq(state->values, std::vector{10, 20});
  pending.request(5);
  run();
  push(40);
  check_eq(state->values, std::vector{10, 20});
  check_eq(state->errors, 0u);
  check_eq(state->completions, 0u);
  check_released();
}

} // WITH_FIXTURE(fixture)
