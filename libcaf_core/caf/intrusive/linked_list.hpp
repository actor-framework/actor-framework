// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/intrusive/forward_iterator.hpp"

#include <memory>
#include <utility>

namespace caf::intrusive {

/// A singly-linked list implementation.
template <class T>
class linked_list {
public:
  // -- member types ----------------------------------------------------------

  using value_type = T;

  using node_type = typename value_type::node_type;

  using node_pointer = node_type*;

  using pointer = value_type*;

  using const_pointer = const value_type*;

  using reference = value_type&;

  using const_reference = const value_type&;

  using unique_pointer = std::unique_ptr<T>;

  using iterator = forward_iterator<value_type>;

  using const_iterator = forward_iterator<const value_type>;

  // -- static utility functions -----------------------------------------------

  /// Casts a node type to its value type.
  static pointer promote(node_pointer ptr) noexcept {
    return static_cast<pointer>(ptr);
  }

  // -- constructors, destructors, and assignment operators -------------------

  linked_list() noexcept = default;

  linked_list(linked_list&& other) noexcept {
    if (!other.empty()) {
      head_.next = other.head_.next;
      tail_.next = other.tail_.next;
      tail_.next->next = &tail_;
      size_ = other.size_;
      other.init();
    }
  }

  linked_list& operator=(linked_list&& other) noexcept {
    clear();
    if (!other.empty()) {
      head_.next = other.head_.next;
      tail_.next = other.tail_.next;
      tail_.next->next = &tail_;
      size_ = other.size_;
      other.init();
    }
    return *this;
  }

  linked_list(const linked_list&) = delete;

  linked_list& operator=(const linked_list&) = delete;

  ~linked_list() {
    clear();
  }

  // -- observers -------------------------------------------------------------

  /// Returns the accumulated size of all stored tasks.
  size_t size() const noexcept {
    return size_;
  }

  /// Returns whether the list has no elements.
  bool empty() const noexcept {
    return size() == 0;
  }

  // -- modifiers -------------------------------------------------------------

  /// Removes all elements from the list.
  void clear() noexcept {
    typename unique_pointer::deleter_type fn;
    drain(fn);
  }

  /// Removes the first element from the list and returns it.
  unique_pointer pop_front() {
    unique_pointer result;
    if (!empty()) {
      auto ptr = promote(head_.next);
      head_.next = ptr->next;
      if (--size_ == 0) {
        CAF_ASSERT(head_.next == &tail_);
        tail_.next = &head_;
      }
      result.reset(ptr);
    }
    return result;
  }

  /// Transfers all elements from `other` into this list.
  void splice(linked_list& other) noexcept {
    if (other.empty()) {
      return;
    }
    tail_.next->next = other.head_.next;
    other.tail_.next->next = &tail_;
    tail_.next = other.tail_.next;
    size_ += other.size_;
    other.init();
  }

  // -- iterator access --------------------------------------------------------

  /// Returns an iterator to the dummy before the first element.
  iterator begin() noexcept {
    return head_.next;
  }

  /// Returns an iterator to the dummy before the first element.
  const_iterator begin() const noexcept {
    return head_.next;
  }

  /// Returns an iterator to the dummy before the first element.
  const_iterator cbegin() const noexcept {
    return begin();
  }

  /// Returns a pointer to the dummy past the last element.
  iterator end() noexcept {
    return &tail_;
  }

  /// Returns a pointer to the dummy past the last element.
  const_iterator end() const noexcept {
    return &tail_;
  }

  /// Returns a pointer to the dummy past the last element.
  const_iterator cend() const noexcept {
    return end();
  }

  /// Returns an iterator to the dummy before the first element.
  iterator before_begin() noexcept {
    return &head_;
  }

  /// Returns an iterator to the last element or to the dummy before the first
  /// element if the list is empty.
  iterator before_end() noexcept {
    return tail_.next;
  }

  // -- element access ---------------------------------------------------------

  /// Returns a pointer to the first element.
  pointer front() noexcept {
    return promote(head_.next);
  }

  /// Returns a pointer to the last element.
  pointer back() noexcept {
    CAF_ASSERT(head_.next != &tail_);
    return promote(tail_.next);
  }

  /// Like `front`, but returns `nullptr` if the list is empty.
  pointer peek() noexcept {
    return size_ > 0 ? front() : nullptr;
  }

  // -- insertion --------------------------------------------------------------

  /// Appends `ptr` to the list.
  /// @pre `ptr != nullptr`
  void push_back(pointer ptr) noexcept {
    CAF_ASSERT(ptr != nullptr);
    tail_.next->next = ptr;
    tail_.next = ptr;
    ptr->next = &tail_;
    ++size_;
  }

  /// Appends `ptr` to the list.
  /// @pre `ptr != nullptr`
  void push_back(unique_pointer ptr) noexcept {
    push_back(ptr.release());
  }

  /// Creates a new element from `xs...` and calls `push_back` with it.
  template <class... Ts>
  void emplace_back(Ts&&... xs) {
    push_back(new value_type(std::forward<Ts>(xs)...));
  }

  void push_front(pointer ptr) noexcept {
    CAF_ASSERT(ptr != nullptr);
    if (empty()) {
      push_back(ptr);
      return;
    }
    ptr->next = head_.next;
    head_.next = ptr;
    ++size_;
  }

  void push_front(unique_pointer ptr) noexcept {
    push_front(ptr.release());
  }

  /// Creates a new element from `xs...` and calls `push_front` with it.
  template <class... Ts>
  void emplace_front(Ts&&... xs) {
    push_front(new value_type(std::forward<Ts>(xs)...));
  }

  /// Inserts `ptr` after `pos`.
  iterator insert_after(iterator pos, pointer ptr) {
    CAF_ASSERT(pos != end());
    CAF_ASSERT(ptr != nullptr);
    auto next = pos->next;
    ptr->next = next;
    pos->next = ptr;
    if (next == &tail_)
      tail_.next = ptr;
    ++size_;
    return iterator{ptr};
  }

  // -- algorithms -------------------------------------------------------------

  /// Tries to find an element in the list that matches the given predicate.
  template <class Predicate>
  pointer find_if(Predicate pred) {
    for (auto i = begin(); i != end(); ++i)
      if (pred(*i))
        return promote(i.ptr);
    return nullptr;
  }

  /// Transfers ownership of all elements to `fn`.
  template <class F>
  void drain(F fn) {
    for (auto i = head_.next; i != &tail_;) {
      auto ptr = i;
      i = i->next;
      fn(promote(ptr));
    }
    init();
  }

private:
  /// Restores a consistent, empty state.
  void init() noexcept {
    head_.next = &tail_;
    tail_.next = &head_;
    size_ = 0;
  }

  // -- member variables ------------------------------------------------------

  /// Dummy before-the-first-element node.
  node_type head_{&tail_};

  /// Dummy past-the-last-element node. The `tail_->next` pointer is pointing to
  /// the last element in the list or to `head_` if the list is empty.
  node_type tail_{&head_};

  /// Stores the total size of all items in the list.
  size_t size_ = 0;
};

} // namespace caf::intrusive
