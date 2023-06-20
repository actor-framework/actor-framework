// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE actor_group

#include "caf/scheduled_actor.hpp"

#include "core-test.hpp"

#include "caf/all.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {

class abstract_mailbox {
public:
  virtual ~abstract_mailbox();

private:
};

class shared_mailbox {
public:
  using node_type = intrusive::singly_linked<mailbox_element>;

  shared_mailbox() {
    ready_workers_.reserve(16);
    tail_ = &head_;
  }

  /// Offers a new message to the worker group. Retrieves a worker from the
  /// cache if possible, otherwise stores the message in the mailbox.
  strong_actor_ptr push(mailbox_element* msg) {
    std::lock_guard guard{mtx_};
    if (!ready_workers_.empty()) {
      auto result = std::move(ready_workers_.back());
      ready_workers_.pop_back();
      return result;
    }
    tail_->next = msg;
    tail_ = msg;
    return nullptr;
  }

  /// Tries to retrieve a message from the mailbox.
  mailbox_element_ptr pull(actor_control_block* worker) {
    std::lock_guard guard{mtx_};
    if (head_.next == nullptr) {
      ready_workers_.push_back(worker);
      return nullptr;
    }
    auto res = mailbox_element_ptr{static_cast<mailbox_element*>(head_.next)};
    head_.next = res->next;
    if (head_.next == nullptr)
      tail_ = &head_;
    return res;
  }

private:
  std::mutex mtx_;
  std::vector<strong_actor_ptr> ready_workers_;
  node_type head_;
  node_type* tail_;
};

class mailbox_cache {
public:
  void push_back(mailbox_element_ptr ptr) {
    if (head_ == nullptr) {
      head_ = ptr.release();
      tail_ = head_;
      return;
    }
    auto next = ptr.release();
    tail_->next = next;
    tail_ = next;
  }

  mailbox_element_ptr pop_front() {
    if (head_ == nullptr)
      return nullptr;
    auto result = mailbox_element_ptr{head_};
    head_ = static_cast<mailbox_element*>(head_->next);
    if (head_ == nullptr)
      tail_ = nullptr;
    return result;
  }

  bool empty() const noexcept {
    return head_ == nullptr;
  }

private:
  mailbox_element* head_;
  mailbox_element* tail_;
};

class CAF_CORE_EXPORT actor_group_dispatcher : public monitorable_actor, public resumable {
public:
  // -- overridden functions of monitorable_actor ------------------------------

  bool enqueue(mailbox_element_ptr what, execution_unit* eu) override;

  void on_destroy() override;

  void on_cleanup(const error& reason) override;

private:
  void handle(execution_unit* eu, mailbox_element_ptr& ptr);

  /// Stores incoming messages.
  shared_mailbox mailbox_;
};

bool actor_group_dispatcher::enqueue(mailbox_element_ptr ptr,
                                     execution_unit* eu) {
  auto worker = mailbox_.offer(ptr.get());
  if (worker != nullptr) {
    worker->enqueue(std::move(ptr), eu);
    return true;
  }
  ptr.release(); // The mailbox took ownership.
  return true;
}

void actor_group_dispatcher::on_destroy() {
  // nop
}

void actor_group_dispatcher::on_cleanup(const error& reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
}

} // namespace caf

using namespace caf;

namespace {

struct fixture {
  fixture() {
  }

  ~fixture() {
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

TEST_CASE("round-robin actor group") {
}

END_FIXTURE_SCOPE()

