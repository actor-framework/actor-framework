// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/disposable.hpp"

#include "caf/ref_counted.hpp"

#include <algorithm>

namespace caf {

// -- member types -------------------------------------------------------------

disposable::impl::~impl() {
  // nop
}

disposable disposable::impl::as_disposable() noexcept {
  return disposable{intrusive_ptr<impl>{this, add_ref}};
}

// -- factories ----------------------------------------------------------------

namespace {

class composite_impl : public ref_counted, public disposable::impl {
public:
  using disposable_list = std::vector<disposable>;

  composite_impl(disposable_list entries) : entries_(std::move(entries)) {
    // nop
  }

  void dispose() override {
    for (auto& entry : entries_)
      entry.dispose();
  }

  bool disposed() const noexcept override {
    auto is_disposed = [](const disposable& entry) { return entry.disposed(); };
    return std::ranges::all_of(entries_, is_disposed);
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

private:
  std::vector<disposable> entries_;
};

} // namespace

disposable disposable::make_composite(std::vector<disposable> entries) {
  if (entries.empty())
    return {};
  return {new composite_impl(std::move(entries)), adopt_ref};
}

namespace {

class flag_impl : public ref_counted, public disposable::impl {
public:
  flag_impl() : flag_(false) {
    // nop
  }

  void dispose() override {
    flag_ = true;
  }

  bool disposed() const noexcept override {
    return flag_.load();
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

private:
  std::atomic<bool> flag_;
};

} // namespace

disposable disposable::make_flag() {
  return {new flag_impl, adopt_ref};
}

// -- utility ------------------------------------------------------------------

size_t disposable::erase_disposed(std::vector<disposable>& xs) {
  auto is_disposed = [](auto& hdl) { return hdl.disposed(); };
  auto xs_end = xs.end();
  if (auto e = std::remove_if(xs.begin(), xs_end, is_disposed); e != xs_end) {
    auto res = std::distance(e, xs_end);
    xs.erase(e, xs_end);
    return static_cast<size_t>(res);
  } else {
    return 0;
  }
}

} // namespace caf
