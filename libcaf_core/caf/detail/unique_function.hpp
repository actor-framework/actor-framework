// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>
#include <utility>

namespace caf::detail {

/// A move-only replacement for `std::function`.
template <class Signature>
class unique_function;

template <class R, class... Ts>
class unique_function<R(Ts...)> {
public:
  // -- member types -----------------------------------------------------------

  /// Function object that dispatches application with a virtual member
  /// function.
  class wrapper {
  public:
    virtual ~wrapper() {
      // nop
    }

    virtual R operator()(Ts...) = 0;
  };

  /// Native function pointer.
  using raw_pointer = R (*)(Ts...);

  /// Pointer to a function wrapper.
  using wrapper_pointer = wrapper*;

  // -- factory functions ------------------------------------------------------

  /// Creates a new function object wrapper.
  template <class F>
  static wrapper_pointer make_wrapper(F&& f) {
    class impl final : public wrapper {
    public:
      impl(F&& fun) : fun_(std::move(fun)) {
        // nop
      }

      R operator()(Ts... xs) override {
        return fun_(xs...);
      }

    private:
      F fun_;
    };
    return new impl(std::forward<F>(f));
  }

  // -- constructors, destructors, and assignment operators --------------------

  unique_function() : holds_wrapper_(false), fptr_(nullptr) {
    // nop
  }

  unique_function(unique_function&& other)
    : holds_wrapper_(other.holds_wrapper_) {
    fptr_ = other.fptr_;
    if (other.holds_wrapper_)
      other.holds_wrapper_ = false;
    other.fptr_ = nullptr;
  }

  unique_function(const unique_function&) = delete;

  explicit unique_function(raw_pointer fun)
    : holds_wrapper_(false), fptr_(fun) {
    // nop
  }

  explicit unique_function(wrapper_pointer ptr)
    : holds_wrapper_(true), wptr_(ptr) {
    // nop
  }

  template <class T,
            class = std::enable_if_t<
              !std::is_convertible_v<T, raw_pointer>
              && std::is_same_v<
                decltype((std::declval<T&>())(std::declval<Ts>()...)), R>>>
  explicit unique_function(T f) : unique_function(make_wrapper(std::move(f))) {
    // nop
  }

  ~unique_function() {
    destroy();
  }

  // -- assignment -------------------------------------------------------------

  unique_function& operator=(unique_function&& other) {
    destroy();
    if (other.holds_wrapper_) {
      holds_wrapper_ = true;
      wptr_ = other.wptr_;
      other.holds_wrapper_ = false;
      other.fptr_ = nullptr;
    } else {
      holds_wrapper_ = false;
      fptr_ = other.fptr_;
    }
    return *this;
  }

  unique_function& operator=(raw_pointer f) {
    return *this = unique_function{f};
  }

  unique_function& operator=(const unique_function&) = delete;

  void assign(raw_pointer f) {
    *this = unique_function{f};
  }

  void assign(wrapper_pointer ptr) {
    *this = unique_function{ptr};
  }

  template <class Fn>
  void emplace(Fn fn) {
    destroy();
    if constexpr (std::is_convertible_v<Fn, raw_pointer>) {
      holds_wrapper_ = false;
      fptr_ = fn;
    } else {
      holds_wrapper_ = true;
      new (&wptr_) wrapper_pointer{make_wrapper(std::move(fn))};
    }
  }

  // -- properties -------------------------------------------------------------

  bool is_nullptr() const noexcept {
    // No type dispatching needed, because the union puts both pointers into
    // the same memory location.
    return fptr_ == nullptr;
  }

  bool holds_wrapper() const noexcept {
    return holds_wrapper_;
  }

  // -- operators --------------------------------------------------------------

  R operator()(Ts... xs) {
    if (holds_wrapper_)
      return (*wptr_)(std::move(xs)...);
    return (*fptr_)(std::move(xs)...);
  }

  explicit operator bool() const noexcept {
    // No type dispatching needed, because the union puts both pointers into
    // the same memory location.
    return !is_nullptr();
  }

  bool operator!() const noexcept {
    return is_nullptr();
  }

private:
  // -- destruction ------------------------------------------------------------

  /// Destroys the managed wrapper.
  void destroy() {
    if (holds_wrapper_)
      delete wptr_;
  }

  // -- member variables -------------------------------------------------------

  bool holds_wrapper_;

  union {
    raw_pointer fptr_;
    wrapper_pointer wptr_;
  };
};

template <class T>
bool operator==(const unique_function<T>& x, std::nullptr_t) noexcept {
  return x.is_nullptr();
}

template <class T>
bool operator==(std::nullptr_t, const unique_function<T>& x) noexcept {
  return x.is_nullptr();
}

template <class T>
bool operator!=(const unique_function<T>& x, std::nullptr_t) noexcept {
  return !x.is_nullptr();
}

template <class T>
bool operator!=(std::nullptr_t, const unique_function<T>& x) noexcept {
  return !x.is_nullptr();
}

} // namespace caf::detail
