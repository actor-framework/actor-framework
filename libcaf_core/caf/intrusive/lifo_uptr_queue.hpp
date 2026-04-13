// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <memory>

namespace caf::intrusive {

/// A LIFO queue based on a singly linked list, where the intrusive `next`
/// pointer is a `std::unique_ptr<T>`.
template <class T>
class lifo_uptr_queue {
public:
  lifo_uptr_queue() = default;

  lifo_uptr_queue(const lifo_uptr_queue&) = delete;

  lifo_uptr_queue& operator=(const lifo_uptr_queue&) = delete;

  void swap(lifo_uptr_queue& other) noexcept {
    head_.swap(other.head_);
  }

  void push(std::unique_ptr<T>&& ptr) {
    ptr->next = std::move(head_);
    head_ = std::move(ptr);
  }

  std::unique_ptr<T> pop() noexcept {
    if (!head_) {
      return nullptr;
    }
    auto result = std::move(head_);
    head_ = std::move(result->next);
    return result;
  }

  bool empty() const noexcept {
    return head_ == nullptr;
  }

  template <class Predicate>
  size_t erase_if(Predicate&& pred) noexcept {
    size_t erased = 0;
    while (head_ && pred(*head_)) {
      auto next = std::move(head_->next);
      head_ = std::move(next);
      ++erased;
    }
    if (!head_) {
      return erased;
    }
    auto* prev = &head_;
    auto* current = &(head_->next);
    while (*current) {
      if (pred(**current)) {
        auto next = std::move((*current)->next);
        (*prev)->next = std::move(next);
        current = &((*prev)->next);
        ++erased;
      } else {
        prev = current;
        current = &((*current)->next);
      }
    }
    return erased;
  }

  template <class Predicate>
  bool erase_first_if(Predicate&& pred) noexcept {
    if (!head_) {
      return false;
    }
    if (pred(*head_)) {
      auto next = std::move(head_->next);
      head_ = std::move(next);
      return true;
    }
    auto* prev = &head_;
    auto* current = &(head_->next);
    while (*current) {
      if (pred(**current)) {
        auto next = std::move((*current)->next);
        (*prev)->next = std::move(next);
        current = &((*prev)->next);
        return true;
      } else {
        prev = current;
        current = &((*current)->next);
      }
    }
    return false;
  }

  template <class Predicate>
  bool any_of(Predicate&& pred) const noexcept {
    for (auto* ptr = head_.get(); ptr != nullptr; ptr = ptr->next.get()) {
      if (pred(*ptr)) {
        return true;
      }
    }
    return false;
  }

  template <class UnaryFunc>
  void for_each(UnaryFunc&& func) noexcept {
    for (auto* ptr = head_.get(); ptr != nullptr; ptr = ptr->next.get()) {
      func(*ptr);
    }
  }

  const std::unique_ptr<T>& top() const noexcept {
    return head_;
  }

private:
  std::unique_ptr<T> head_;
};

} // namespace caf::intrusive
