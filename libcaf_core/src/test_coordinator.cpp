/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/scheduler/test_coordinator.hpp"

#include <limits>

#include "caf/resumable.hpp"
#include "caf/monitorable_actor.hpp"

namespace caf {
namespace scheduler {

namespace {

class dummy_worker : public execution_unit {
public:
  dummy_worker(test_coordinator* parent)
      : execution_unit(&parent->system()),
        parent_(parent) {
    // nop
  }

  void exec_later(resumable* ptr, bool) override {
    parent_->jobs.push_back(ptr);
  }

  bool is_neighbor(execution_unit*) const override {
    return false; 
  }

private:
  test_coordinator* parent_;
};

class dummy_printer : public monitorable_actor {
public:
  dummy_printer(actor_config& cfg) : monitorable_actor(cfg) {
    mh_.assign(
      [&](add_atom, actor_id, const std::string& str) {
        std::cout << str;
      }
    );
  }

  void enqueue(mailbox_element_ptr what, execution_unit*) override {
    mh_(what->content());
  }

private:
  message_handler mh_;
};

class dummy_timer : public monitorable_actor {
public:
  dummy_timer(actor_config& cfg, test_coordinator* parent)
      : monitorable_actor(cfg),
        parent_(parent) {
    mh_.assign(
      [&](const duration& d, strong_actor_ptr& from,
          strong_actor_ptr& to, message_id mid, message& msg) {
        auto tout = test_coordinator::hrc::now();
        tout += d;
        using delayed_msg = test_coordinator::delayed_msg;
        parent_->delayed_messages.emplace(tout, delayed_msg{std::move(from),
                                                            std::move(to), mid,
                                                            std::move(msg)});
      }
    );
  }

  void enqueue(mailbox_element_ptr what, execution_unit*) override {
    mh_(what->content());
  }

private:
  test_coordinator* parent_;
  message_handler mh_;
};

} // namespace <anonymous>

test_coordinator::test_coordinator(actor_system& sys) : super(sys) {
  // nop
}
void test_coordinator::start() {
  dummy_worker worker{this};
  actor_config cfg{&worker};
  auto& sys = system();
  timer_ = make_actor<dummy_timer, strong_actor_ptr>(
    sys.next_actor_id(), sys.node(), &sys, cfg, this);
  printer_ = make_actor<dummy_printer, strong_actor_ptr>(
    sys.next_actor_id(), sys.node(), &sys, cfg);
}

void test_coordinator::stop() {
  run_dispatch_loop();
}

void test_coordinator::enqueue(resumable* ptr) {
  CAF_LOG_TRACE("");
  jobs.push_back(ptr);
  if (after_next_enqueue_ != nullptr) {
    std::function<void()> f;
    f.swap(after_next_enqueue_);
    f();
  }
}

bool test_coordinator::try_run_once() {
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

bool test_coordinator::dispatch_once() {
  auto i = delayed_messages.begin();
  if (i == delayed_messages.end())
    return false;
  auto& dm = i->second;
  dm.to->enqueue(dm.from, dm.mid, std::move(dm.msg), nullptr);
  delayed_messages.erase(i);
  return true;
}

size_t test_coordinator::dispatch() {
  size_t res = 0;
  while (dispatch_once())
    ++res;
  return res;
}

std::pair<size_t, size_t> test_coordinator::run_dispatch_loop() {
  std::pair<size_t, size_t> res;
  size_t i = 0;
  do {
    auto x = run();
    auto y = dispatch();
    res.first += x;
    res.second += y;
    i = x + y;
  } while (i > 0);
  return res;
}

void test_coordinator::inline_next_enqueue() {
  after_next_enqueue([=] { run_once_lifo(); });
}

void test_coordinator::inline_all_enqueues() {
  after_next_enqueue([=] { inline_all_enqueues_helper(); });
}

void test_coordinator::inline_all_enqueues_helper() {
  run_once_lifo();
  after_next_enqueue([=] { inline_all_enqueues_helper(); });
}

} // namespace caf
} // namespace scheduler

