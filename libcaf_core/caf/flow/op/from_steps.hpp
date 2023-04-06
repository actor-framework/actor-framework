// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/defaults.hpp"
#include "caf/detail/plain_ref_counted.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/type_list.hpp"

#include <deque>
#include <tuple>
#include <utility>

namespace caf::flow::op {

template <class... Steps>
using from_steps_output_t =
  typename detail::tl_back_t<detail::type_list<Steps...>>::output_type;

template <class Input, class... Steps>
class from_steps_sub : public detail::plain_ref_counted,
                       public observer_impl<Input>,
                       public subscription_impl {
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
      if (sub->in_) {
        auto tmp = std::move(sub->in_);
        tmp.dispose();
      }
    }

    void on_error(const error& what) {
      if (sub->in_) {
        auto tmp = std::move(sub->in_);
        tmp.dispose();
      }
      sub->err_ = what;
    }
  };

  // -- constructors, destructors, and assignment operators --------------------

  from_steps_sub(coordinator* ctx, observer<output_type> out,
                 std::tuple<Steps...> steps)
    : ctx_(ctx), out_(std::move(out)), steps_(std::move(steps)) {
    // nop
  }

  // -- ref counting -----------------------------------------------------------

  void ref_disposable() const noexcept final {
    this->ref();
  }

  void deref_disposable() const noexcept final {
    this->deref();
  }

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

  bool subscribed() const noexcept {
    return in_.valid();
  }

  const error& fail_reason() const {
    return err_;
  }

  bool idle() {
    return demand_ > 0 && buf_.empty();
  }

  // -- implementation of observer_impl<Input> ---------------------------------

  void on_next(const Input& item) override {
    CAF_ASSERT(!in_ || in_flight_ > 0);
    if (in_) {
      --in_flight_;
      auto fn = [this, &item](auto& step, auto&... steps) {
        term_step term{this};
        step.on_next(item, steps..., term);
      };
      std::apply(fn, steps_);
      pull();
      if (!running_) {
        running_ = true;
        do_run();
      }
    }
  }

  void on_complete() override {
    if (in_) {
      auto fn = [this](auto& step, auto&... steps) {
        term_step term{this};
        step.on_complete(steps..., term);
      };
      std::apply(fn, steps_);
      if (!running_) {
        running_ = true;
        do_run();
      }
    }
  }

  void on_error(const error& what) override {
    if (in_) {
      if (!err_) {
        auto fn = [this, &what](auto& step, auto&... steps) {
          term_step term{this};
          step.on_error(what, steps..., term);
        };
        std::apply(fn, steps_);
        if (!running_) {
          running_ = true;
          do_run();
        }
      }
    } else if (out_) {
      // This may only happen if subscribing to the input fails.
      auto fn = [this, &what](auto& step, auto&... steps) {
        term_step term{this};
        step.on_error(what, steps..., term);
      };
      std::apply(fn, steps_);
      if (!running_) {
        running_ = true;
        do_run();
      }
    }
  }

  void on_subscribe(subscription in) override {
    if (!in_) {
      in_ = std::move(in);
      pull();
    } else {
      in.dispose();
    }
  }

  // -- implementation of subscription_impl ------------------------------------

  bool disposed() const noexcept override {
    return disposed_;
  }

  void dispose() override {
    CAF_LOG_TRACE("");
    if (!disposed_) {
      disposed_ = true;
      demand_ = 0;
      buf_.clear();
      ctx_->delay_fn([out = std::move(out_)]() mutable { out.on_complete(); });
    }
    if (in_) {
      auto tmp = std::move(in_);
      tmp.dispose();
    }
  }

  void request(size_t n) override {
    CAF_LOG_TRACE(CAF_ARG(n));
    if (demand_ != 0) {
      demand_ += n;
    } else {
      demand_ = n;
      run_later();
    }
  }

private:
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
      running_ = true;
      ctx_->delay_fn([ptr = strong_this()] { ptr->do_run(); });
    }
  }

  void do_run() {
    auto guard = detail::make_scope_guard([this] { running_ = false; });
    if (!disposed_) {
      CAF_ASSERT(out_);
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
      } else if (buf_.empty()) {
        disposed_ = true;
        auto tmp = std::move(out_);
        if (err_)
          tmp.on_error(err_);
        else
          tmp.on_complete();
      }
    }
  }

  intrusive_ptr<from_steps_sub> strong_this() {
    return {this};
  }

  coordinator* ctx_;
  subscription in_;
  observer<output_type> out_;
  std::tuple<Steps...> steps_;
  std::deque<output_type> buf_;
  size_t demand_ = 0;
  size_t in_flight_ = 0;
  size_t max_buf_size_ = defaults::flow::buffer_size;
  bool disposed_ = false;
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

  from_steps(coordinator* ctx, intrusive_ptr<base<input_type>> input,
             std::tuple<Steps...> steps)
    : super(ctx), input_(std::move(input)), steps_(std::move(steps)) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<output_type> out) override {
    using sub_t = from_steps_sub<Input, Steps...>;
    auto ptr = make_counted<sub_t>(super::ctx_, out, steps_);
    input_->subscribe(observer<input_type>{ptr});
    if (ptr->subscribed()) {
      out.on_subscribe(subscription{ptr});
      return ptr->as_disposable();
    }
    return disposable{};
  }

private:
  intrusive_ptr<base<input_type>> input_;
  std::tuple<Steps...> steps_;
};

} // namespace caf::flow::op
