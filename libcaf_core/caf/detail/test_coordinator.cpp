#include "caf/detail/test_coordinator.hpp"

#include "caf/disposable.hpp"

namespace caf::detail {

test_actor_clock::time_point test_actor_clock::now() const noexcept {
  return current_time;
}

caf::disposable test_actor_clock::schedule(time_point abs_time, caf::action f) {
  CAF_ASSERT(f.ptr() != nullptr);
  actions.emplace(abs_time, f);
  return std::move(f).as_disposable();
}

bool test_actor_clock::trigger_timeout() {
  for (;;) {
    if (actions.empty())
      return false;
    auto i = actions.begin();
    auto t = i->first;
    if (t > current_time)
      current_time = t;
    if (try_trigger_once())
      return true;
  }
}

size_t test_actor_clock::trigger_timeouts() {
  if (actions.empty())
    return 0u;
  size_t result = 0;
  while (trigger_timeout())
    ++result;
  return result;
}

size_t test_actor_clock::advance_time(duration_type x) {
  current_time += x;
  auto result = size_t{0};
  while (!actions.empty() && actions.begin()->first <= current_time)
    if (try_trigger_once())
      ++result;
  return result;
}

test_actor_clock::time_point test_actor_clock::next_timeout() const {
  return actions.begin()->first;
}

bool test_actor_clock::try_trigger_once() {
  for (;;) {
    if (actions.empty())
      return false;
    auto i = actions.begin();
    auto [t, f] = *i;
    if (t > current_time)
      return false;
    actions.erase(i);
    if (!f.disposed()) {
      f.run();
      return true;
    }
  }
}

namespace {

class dummy_worker : public caf::scheduler {
public:
  dummy_worker(test_coordinator* parent) : parent_(parent) {
    // nop
  }

  void schedule(caf::resumable* ptr) override {
    parent_->jobs.push_back(ptr);
  }

  void delay(caf::resumable* ptr) override {
    parent_->jobs.push_back(ptr);
  }

  void start() override {
    // nop
  }

  void stop() override {
    // nop
  }

private:
  test_coordinator* parent_;
};

} // namespace

bool test_coordinator::prioritize_impl(caf::resumable* ptr) {
  if (!ptr)
    return false;
  auto i = std::find(jobs.begin(), jobs.end(), ptr);
  if (i == jobs.end())
    return false;
  if (i == jobs.begin())
    return true;
  std::rotate(jobs.begin(), i, i + 1);
  return true;
}

bool test_coordinator::try_run_once() {
  if (jobs.empty())
    return false;
  auto job = jobs.front();
  jobs.pop_front();
  dummy_worker worker{this};
  switch (job->resume(&worker, 1)) {
    case caf::resumable::resume_later:
      jobs.push_front(job);
      break;
    case caf::resumable::done:
    case caf::resumable::awaiting_message:
      intrusive_ptr_release(job);
      break;
    case caf::resumable::shutdown_execution_unit:
      break;
  }
  return true;
}

bool test_coordinator::try_run_once_lifo() {
  if (jobs.empty())
    return false;
  if (jobs.size() >= 2)
    std::rotate(jobs.rbegin(), jobs.rbegin() + 1, jobs.rend());
  return try_run_once();
}

void test_coordinator::run_once() {
  if (jobs.empty())
    CAF_RAISE_ERROR("No job to run available.");
  try_run_once();
}

void test_coordinator::run_once_lifo() {
  if (jobs.empty())
    CAF_RAISE_ERROR("No job to run available.");
  try_run_once_lifo();
}

size_t test_coordinator::run(size_t max_count) {
  size_t res = 0;
  while (res < max_count && try_run_once())
    ++res;
  return res;
}

bool test_coordinator::has_pending_timeout() {
  auto& clock = dynamic_cast<test_actor_clock&>(system().clock());
  return clock.has_pending_timeout();
}

bool test_coordinator::trigger_timeout() {
  auto& clock = dynamic_cast<test_actor_clock&>(system().clock());
  return clock.trigger_timeout();
}

size_t test_coordinator::trigger_timeouts() {
  auto& clock = dynamic_cast<test_actor_clock&>(system().clock());
  return clock.trigger_timeouts();
}

size_t test_coordinator::advance_time(caf::timespan x) {
  auto& clock = dynamic_cast<test_actor_clock&>(system().clock());
  return clock.advance_time(x);
}

void test_coordinator::inline_next_enqueue() {
  after_next_enqueue([this] { run_once_lifo(); });
}

void test_coordinator::inline_all_enqueues() {
  after_next_enqueue([this] { inline_all_enqueues_helper(); });
}

test_actor_clock& test_coordinator::test_coordinator::clock() {
  return dynamic_cast<test_actor_clock&>(system().clock());
}

void test_coordinator::start() {
  // nop
}

void test_coordinator::stop() {
  while (run() > 0)
    trigger_timeouts();
}

void test_coordinator::schedule(caf::resumable* ptr) {
  jobs.push_back(ptr);
  if (after_next_enqueue_ != nullptr) {
    std::function<void()> f;
    f.swap(after_next_enqueue_);
    f();
  }
}

void test_coordinator::delay(caf::resumable* ptr) {
  schedule(ptr);
}

void test_coordinator::inline_all_enqueues_helper() {
  after_next_enqueue([this] { inline_all_enqueues_helper(); });
  run_once_lifo();
}

} // namespace caf::detail
