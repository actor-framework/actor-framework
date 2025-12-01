// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/counted_disposable.hpp"

#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"

namespace caf::detail {

class counted_disposable::nested_disposable : public ref_counted,
                                              public disposable::impl {
public:
  explicit nested_disposable(intrusive_ptr<counted_disposable> parent)
    : parent_(std::move(parent)) {
    // nop
  }

  ~nested_disposable() override {
    dispose();
  }

  void dispose() override {
    if (parent_) {
      parent_->release();
      parent_ = nullptr;
    }
  }

  bool disposed() const noexcept override {
    return parent_ == nullptr;
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  friend void intrusive_ptr_add_ref(const nested_disposable* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const nested_disposable* ptr) noexcept {
    ptr->deref();
  }

private:
  intrusive_ptr<counted_disposable> parent_;
};

disposable counted_disposable::acquire() {
  ++count_;
  return disposable{
    make_counted<nested_disposable>(intrusive_ptr<counted_disposable>(this))};
}

void counted_disposable::dispose() {
  decorated_.dispose();
}

bool counted_disposable::disposed() const noexcept {
  return decorated_.disposed();
}

void counted_disposable::ref_disposable() const noexcept {
  ref();
}

void counted_disposable::deref_disposable() const noexcept {
  deref();
}

void counted_disposable::release() {
  if (--count_ == 0) {
    decorated_.dispose();
  }
}

} // namespace caf::detail
