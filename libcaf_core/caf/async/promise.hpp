// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/execution_context.hpp"
#include "caf/async/future.hpp"
#include "caf/detail/async_cell.hpp"
#include "caf/disposable.hpp"
#include "caf/raise_error.hpp"

namespace caf::async {

/// Provides a facility to store a value or an error that is later acquired
/// asynchronously via a @ref future object. A promise may deliver only one
/// value.
template <class T>
class promise {
public:
  using value_type = std::conditional_t<std::is_void_v<T>, unit_t, T>;

  promise(promise&&) noexcept = default;

  promise& operator=(promise&&) noexcept = default;

  promise(const promise& other) noexcept : promise(other.cell_) {
    // nop
  }

  promise& operator=(const promise& other) noexcept {
    if (this != &other)
      return *this = promise{other};
    return *this;
  }

  promise() : cell_(std::make_shared<cell_type>()) {
    // nop
  }

  ~promise() {
    if (valid()) {
      auto& cnt = cell_->promises;
      if (cnt == 1 || cnt.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        typename cell_type::event_list events;
        { // Critical section.
          std::unique_lock guard{cell_->mtx};
          if (std::holds_alternative<none_t>(cell_->value)) {
            cell_->value = make_error(sec::broken_promise);
            cell_->events.swap(events);
          }
        }
        for (auto& [listener, callback] : events) {
          if (listener)
            listener->schedule(std::move(callback));
          else
            callback.run();
        }
      }
    }
  }

  bool valid() const noexcept {
    return cell_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  bool operator!() const noexcept {
    return !valid();
  }

  /// @pre `valid()`
  void set_value(value_type value) {
    if (valid()) {
      do_set(value);
      cell_ = nullptr;
    }
  }

  /// @pre `valid()`
  void set_error(error reason) {
    if (valid()) {
      do_set(reason);
      cell_ = nullptr;
    }
  }

  /// @pre `valid()`
  future<T> get_future() const {
    return future<T>{cell_};
  }

private:
  using cell_type = detail::async_cell<T>;
  using cell_ptr = std::shared_ptr<cell_type>;

  explicit promise(cell_ptr cell) noexcept : cell_(std::move(cell)) {
    CAF_ASSERT(cell_ != nullptr);
    cell_->promises.fetch_add(1, std::memory_order_relaxed);
  }

  template <class What>
  void do_set(What& what) {
    typename cell_type::event_list events;
    { // Critical section.
      std::unique_lock guard{cell_->mtx};
      if (std::holds_alternative<none_t>(cell_->value)) {
        cell_->value = std::move(what);
        cell_->events.swap(events);
      } else {
        CAF_RAISE_ERROR("promise already satisfied");
      }
    }
    for (auto& [listener, callback] : events) {
      if (listener)
        listener->schedule(std::move(callback));
      else
        callback.run();
    }
  }

  cell_ptr cell_;
};

} // namespace caf::async
