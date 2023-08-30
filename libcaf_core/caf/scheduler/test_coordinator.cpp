// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/scheduler/test_coordinator.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/monitorable_actor.hpp"
#include "caf/raise_error.hpp"
#include "caf/resumable.hpp"

#include <limits>

namespace caf::scheduler {

namespace {

class dummy_worker : public execution_unit {
public:
  dummy_worker(test_coordinator* parent)
    : execution_unit(&parent->system()), parent_(parent) {
    // nop
  }

  void exec_later(resumable* ptr) override {
    parent_->jobs.push_back(ptr);
  }

private:
  test_coordinator* parent_;
};

class dummy_printer : public monitorable_actor {
public:
  dummy_printer(actor_config& cfg) : monitorable_actor(cfg) {
    mh_.assign(
      [&](add_atom, actor_id, const std::string& str) { std::cout << str; });
  }

  bool enqueue(mailbox_element_ptr what, execution_unit*) override {
    mh_(what->content());
    return true;
  }

  void setup_metrics() {
    // nop
  }

private:
  message_handler mh_;
};

} // namespace

test_coordinator::test_coordinator(actor_system& sys) : super(sys) {
  // nop
}

bool test_coordinator::detaches_utility_actors() const {
  return false;
}

detail::test_actor_clock& test_coordinator::clock() noexcept {
  return clock_;
}

void test_coordinator::start() {
  CAF_LOG_TRACE("");
  dummy_worker worker{this};
  actor_config cfg{&worker};
  auto& sys = system();
  utility_actors_[printer_id] = make_actor<dummy_printer, actor>(
    sys.next_actor_id(), sys.node(), &sys, cfg);
}

void test_coordinator::stop() {
  CAF_LOG_TRACE("");
  while (run() > 0)
    trigger_timeouts();
}

void test_coordinator::enqueue(resumable* ptr) {
  CAF_LOG_TRACE("");
  jobs.push_back(ptr);
  if (after_next_enqueue_ != nullptr) {
    CAF_LOG_DEBUG("inline this enqueue");
    std::function<void()> f;
    f.swap(after_next_enqueue_);
    f();
  }
}

bool test_coordinator::try_run_once() {
  CAF_LOG_TRACE("");
  if (jobs.empty())
    return false;
  auto job = jobs.front();
  jobs.pop_front();
  dummy_worker worker{this};
  switch (job->resume(&worker, 1)) {
    case resumable::resume_later:
      jobs.push_front(job);
      break;
    case resumable::done:
    case resumable::awaiting_message:
      intrusive_ptr_release(job);
      break;
    case resumable::shutdown_execution_unit:
      break;
  }
  return true;
}

bool test_coordinator::try_run_once_lifo() {
  CAF_LOG_TRACE("");
  if (jobs.empty())
    return false;
  if (jobs.size() >= 2)
    std::rotate(jobs.rbegin(), jobs.rbegin() + 1, jobs.rend());
  return try_run_once();
}

void test_coordinator::run_once() {
  CAF_LOG_TRACE("");
  if (jobs.empty())
    CAF_RAISE_ERROR("No job to run available.");
  try_run_once();
}

void test_coordinator::run_once_lifo() {
  CAF_LOG_TRACE("");
  if (jobs.empty())
    CAF_RAISE_ERROR("No job to run available.");
  try_run_once_lifo();
}

size_t test_coordinator::run(size_t max_count) {
  CAF_LOG_TRACE(CAF_ARG(max_count));
  size_t res = 0;
  while (res < max_count && try_run_once())
    ++res;
  return res;
}

void test_coordinator::inline_next_enqueue() {
  CAF_LOG_TRACE("");
  after_next_enqueue([this] { run_once_lifo(); });
}

void test_coordinator::inline_all_enqueues() {
  CAF_LOG_TRACE("");
  after_next_enqueue([this] { inline_all_enqueues_helper(); });
}

void test_coordinator::inline_all_enqueues_helper() {
  CAF_LOG_TRACE("");
  after_next_enqueue([this] { inline_all_enqueues_helper(); });
  run_once_lifo();
}

} // namespace caf::scheduler
