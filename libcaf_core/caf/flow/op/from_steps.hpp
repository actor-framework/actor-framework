// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/defaults.hpp"
#include "caf/detail/plain_ref_counted.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/flow/observer.hpp"

#include <deque>
#include <tuple>
#include <utility>

namespace caf::flow::op {

template <class... Steps>
using from_steps_output_t =
  typename detail::tl_back_t<type_list<Steps...>>::output_type;

template <class Input, class... Steps>
class from_steps_sub : public subscription::impl_base,
                       public observer_impl<Input> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = from_steps_output_t<Steps...>;

  struct term_step;

  friend struct term_step;

  struct term_step {
    from_steps_sub* sub;

    bool on_next(const output_type& next) {
      sub->buf_.push_back(next);
      return true;
    }

    void on_complete() {
      // If a step calls on_complete from on_next, it must return `false`. This
      // will cause on_next on the from_term_sub to dispose its input.
    }

    void on_error(const error& what) {
      // Same as for on_complete, except that we store the error.
      sub->err_ = what;
    }
  };

  // -- constructors, destructors, and assignment operators --------------------

  from_steps_sub(coordinator* parent, observer<output_type> out,
                 std::tuple<Steps...> steps)
    : parent_(parent), out_(std::move(out)), steps_(std::move(steps)) {
    // nop
  }

  // -- ref counting -----------------------------------------------------------

  void ref_coordinated() const noexcept final {
    this->ref();
  }

  void deref_coordinated() const noexcept final {
    this->deref();
  }

  friend void intrusive_ptr_add_ref(const from_steps_sub* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const from_steps_sub* ptr) noexcept {
    ptr->deref();
  }

  // -- properties -------------------------------------------------------------

  const error& fail_reason() const {
    return err_;
  }

  bool idle() {
    return demand_ > 0 && buf_.empty();
  }

  // -- implementation of observer_impl<Input> ---------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  void on_next(const Input& item) override {
    CAF_ASSERT(!in_ || in_flight_ > 0);
    if (!in_)
      return;
    --in_flight_;
    auto fn = [this, &item](auto& step, auto&... steps) {
      term_step term{this};
      return step.on_next(item, steps..., term);
    };
    if (!std::apply(fn, steps_)) {
      in_.cancel();
    } else {
      pull();
    }
    if (!running_)
      do_run();
  }

  void on_complete() override {
    if (!in_)
      return;
    in_.release_later();
    auto fn = [this](auto& step, auto&... steps) {
      term_step term{this};
      step.on_complete(steps..., term);
    };
    std::apply(fn, steps_);
    if (!running_)
      do_run();
  }

  void on_error(const error& what) override {
    if (!in_)
      return;
    in_.release_later();
    auto fn = [this, &what](auto& step, auto&... steps) {
      term_step term{this};
      step.on_error(what, steps..., term);
    };
    std::apply(fn, steps_);
    if (!running_)
      do_run();
  }

  void on_subscribe(subscription in) override {
    if (!in_) {
      in_ = std::move(in);
      pull();
      return;
    }
    in.cancel();
  }

  // -- implementation of subscription_impl ------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t n) override {
    auto lg = log::core::trace("n = {}", n);
    if (demand_ != 0) {
      demand_ += n;
      return;
    }
    demand_ = n;
    run_later();
  }

private:
  void do_dispose(bool from_external) override {
    auto lg = log::core::trace("");
    if (!out_)
      return;
    in_.cancel();
    demand_ = 0;
    buf_.clear();
    if (from_external)
      out_.on_error(make_error(sec::disposed));
    else
      out_.release_later();
  }

  void pull() {
    if (auto pending = buf_.size() + in_flight_;
        in_ && pending < max_buf_size_) {
      auto new_demand = max_buf_size_ - pending;
      in_flight_ += new_demand;
      in_.request(new_demand);
    }
  }

  void run_later() {
    if (!running_) {
      parent_->delay_fn([ptr = strong_this()] { ptr->do_run(); });
    }
  }

  void do_run() {
    running_ = true;
    auto guard = detail::scope_guard{[this]() noexcept { running_ = false; }};
    if (!out_)
      return;
    while (demand_ > 0 && !buf_.empty()) {
      auto item = std::move(buf_.front());
      buf_.pop_front();
      --demand_;
      out_.on_next(item);
      // Note: on_next() may call dispose() and set out_ to nullptr.
      if (!out_)
        return;
    }
    if (in_) {
      pull();
      return;
    }
    if (buf_.empty() && out_) {
      if (!err_)
        out_.on_complete();
      else
        out_.on_error(err_);
    }
  }

  intrusive_ptr<from_steps_sub> strong_this() {
    return {this};
  }

  coordinator* parent_;
  subscription in_;
  observer<output_type> out_;
  std::tuple<Steps...> steps_;
  std::deque<output_type> buf_;
  size_t demand_ = 0;
  size_t in_flight_ = 0;
  size_t max_buf_size_ = defaults::flow::buffer_size;
  bool running_ = false;
  error err_;
};

template <class Input, class... Steps>
class from_steps : public op::cold<from_steps_output_t<Steps...>> {
public:
  // -- member types -----------------------------------------------------------

  static_assert(sizeof...(Steps) > 0);

  using input_type = Input;

  using output_type = from_steps_output_t<Steps...>;

  using super = op::cold<output_type>;

  // -- constructors, destructors, and assignment operators --------------------

  from_steps(coordinator* parent, intrusive_ptr<base<input_type>> input,
             std::tuple<Steps...> steps)
    : super(parent), input_(std::move(input)), steps_(std::move(steps)) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<output_type> out) override {
    using sub_t = from_steps_sub<Input, Steps...>;
    auto ptr = super::parent_->add_child(std::in_place_type<sub_t>, out,
                                         steps_);
    out.on_subscribe(subscription{ptr});
    input_->subscribe(observer<input_type>{ptr});
    return ptr->as_disposable();
  }

private:
  intrusive_ptr<base<input_type>> input_;
  std::tuple<Steps...> steps_;
};

} // namespace caf::flow::op
