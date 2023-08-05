// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::intrusive {

/// A simple stack implementation with singly-linked nodes.
template <class T>
class stack {
public:
  using pointer = T*;

  stack() = default;

  stack(const stack&) = delete;

  stack& operator=(const stack&) = delete;

  ~stack() {
    while (head_) {
      auto next = static_cast<pointer>(head_->next);
      delete head_;
      head_ = next;
    }
  }

  void push(pointer ptr) {
    ptr->next = head_;
    head_ = ptr;
  }

  pointer pop() {
    auto result = head_;
    if (result)
      head_ = static_cast<pointer>(result->next);
    return result;
  }

  bool empty() const noexcept {
    return head_ == nullptr;
  }

private:
  pointer head_ = nullptr;
};

} // namespace caf::intrusive
