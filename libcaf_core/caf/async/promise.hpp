// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/async/future.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/async_cell.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"
#include "caf/raise_error.hpp"

namespace caf::async {

/// Provides a facility to store a value or an error that is later acquired
/// asynchronously via a @ref future object. A promise may deliver only one
/// value.
template <class T>
class promise final {
public:
  using value_type = std::conditional_t<std::is_void_v<T>, unit_t, T>;

  promise(promise&&) noexcept = default;

  promise& operator=(promise&& other) noexcept {
    if (this != &other) {
      if (cell_) {
        cell_->dec_promise_ref();
      }
      cell_ = std::move(other.cell_);
    }
    return *this;
  }

  promise(const promise& other) noexcept : cell_(other.cell_) {
    CAF_ASSERT(cell_ != nullptr);
    cell_->inc_promise_ref();
  }

  promise& operator=(const promise& other) noexcept {
    if (this != &other) {
      promise tmp{other};
      cell_.swap(tmp.cell_);
    }
    return *this;
  }

  promise() : cell_(caf::make_counted<cell_type>()) {
    // nop
  }

  ~promise() {
    if (cell_) {
      cell_->dec_promise_ref();
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
    if (cell_) {
      cell_->set(std::move(value));
      cell_->dec_promise_ref();
      cell_.reset();
    }
  }

  /// @pre `valid()`
  void set_error(error reason) {
    if (cell_) {
      cell_->set(std::move(reason));
      cell_->dec_promise_ref();
      cell_.reset();
    }
  }

  /// Tries to set the dispose callback.
  /// @return `true` if the callback was set successfully, `false` otherwise.
  bool set_on_dispose(execution_context_ptr ctx, action callback) {
    if (cell_) {
      return cell_->set_on_dispose(std::move(ctx), std::move(callback));
    }
    return false;
  }

  /// @copydoc set_on_cancel
  bool set_on_cancel(execution_context* ctx, action callback) {
    return set_on_dispose(execution_context_ptr{ctx, add_ref},
                          std::move(callback));
  }

  /// @pre `valid()`
  future<T> get_future() const {
    return future<T>{cell_};
  }

private:
  using cell_type = detail::async_cell<T>;
  using cell_ptr = intrusive_ptr<cell_type>;

  cell_ptr cell_;
};

} // namespace caf::async
