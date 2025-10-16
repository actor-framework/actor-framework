// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/flow/event.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"

#include <memory>

namespace caf::flow::op {

template <class T>
class cache_sub : public subscription::impl_base {
public:
  using super = subscription::impl_base;

  using input_type = T;

  using output_type = T;

  using cache_type = std::vector<event<T>>;

  cache_sub(coordinator* parent, observer<T> out,
            std::shared_ptr<cache_type> cache)
    : parent_(parent), out_(std::move(out)), cache_(std::move(cache)) {
    // nop
  }

  coordinator* parent() const noexcept override {
    return parent_;
  }

  void request(size_t n) override {
    if (demand_ > 0) {
      demand_ += n;
      return;
    }
    demand_ = n;
    auto self = intrusive_ptr<cache_sub>{this};
    parent_->delay_fn([self] { self->update(); });
  }

  void update() {
    while (out_ && demand_ > 0 && index_ < cache_->size()) {
      --demand_;
      push();
      ++index_;
    }
  }

  bool done() const noexcept {
    return !out_;
  }

  bool disposed() const noexcept override {
    return done();
  }

private:
  void do_dispose(bool from_external) override {
    if (!out_) {
      return;
    }
    if (from_external) {
      out_.on_error(make_error(sec::disposed));
    } else {
      out_.release_later();
    }
  }

  void push() {
    auto fn = [this]<class Event>(Event& ev) {
      if constexpr (std::is_same_v<Event, on_next_event<T>>) {
        out_.on_next(ev.item);
      } else if constexpr (std::is_same_v<Event, on_error_event>) {
        out_.on_error(ev.what);
      } else if constexpr (std::is_same_v<Event, on_complete_event>) {
        out_.on_complete();
      }
    };
    std::visit(fn, cache_->at(index_));
  }

  coordinator* parent_;
  observer<T> out_;
  size_t index_ = 0;
  size_t demand_ = 0;
  std::shared_ptr<cache_type> cache_;
};

template <class T>
class cache : public cold<T>, public observer_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = cold<T>;

  using input_type = T;

  using output_type = T;

  using handle_type = observable<T>;

  using cache_type = std::vector<event<T>>;

  using cache_ptr = std::shared_ptr<cache_type>;

  /// The default initial capacity of the cache. The default value is reasonably
  /// small to avoid unnecessary memory allocation but still enough to avoid
  /// frequent reallocations.
  static constexpr size_t default_initial_capacity = 64;

  cache(coordinator* parent, intrusive_ptr<base<T>> source,
        size_t initial_capacity = default_initial_capacity)
    : super(parent),
      cache_(std::make_shared<cache_type>()),
      source_(std::move(source)) {
    cache_->reserve(initial_capacity);
  }

  coordinator* parent() const noexcept override {
    return super::parent_;
  }

  void on_complete() override {
    cache_->emplace_back(on_complete_event{});
    update();
    subs_.clear();
  }

  void on_error(const error& what) override {
    cache_->emplace_back(on_error_event{what});
    update();
    subs_.clear();
  }

  void on_next(const T& item) override {
    cache_->emplace_back(on_next_event{item});
    update();
    auto is_done = [](const auto& sub) { return sub->done(); };
    subs_.erase(std::remove_if(subs_.begin(), subs_.end(), is_done),
                subs_.end());
    sub_.request(1);
  }

  void on_subscribe(subscription new_sub) override {
    if (sub_) {
      new_sub.cancel();
      return;
    }
    sub_ = std::move(new_sub);
    auto demand = cache_->capacity();
    sub_.request(demand > 0 ? demand : default_initial_capacity);
  }

  void subscribe_to_source() {
    if (source_) {
      source_->subscribe(this->as_observer());
      source_ = nullptr;
    }
  }

  disposable subscribe(observer<T> out) override {
    subscribe_to_source();
    auto ptr = super::parent_->add_child(std::in_place_type<cache_sub<T>>, out,
                                         cache_);
    out.on_subscribe(subscription{ptr});
    if (!done()) {
      subs_.push_back(ptr);
    }
    return disposable{std::move(ptr)};
  }

  bool done() const noexcept {
    if (!cache_->empty()) {
      return !std::holds_alternative<on_next_event<T>>(cache_->back());
    }
    return false;
  }

  size_t cached_events() const noexcept {
    return cache_->size();
  }

  void ref_coordinated() const noexcept final {
    super::ref();
  }

  void deref_coordinated() const noexcept final {
    super::deref();
  }

  friend void intrusive_ptr_add_ref(const cache* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const cache* ptr) noexcept {
    ptr->deref();
  }

private:
  std::shared_ptr<cache_type> cache_;
  subscription sub_;
  std::vector<intrusive_ptr<cache_sub<T>>> subs_;
  intrusive_ptr<base<T>> source_;

  void update() {
    for (auto& sub : subs_) {
      sub->update();
    }
  }
};

} // namespace caf::flow::op
