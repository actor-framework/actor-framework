// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/sec.hpp"

#include <tuple>
#include <utility>

namespace caf::flow::op {

template <class Generator, class... Steps>
class from_generator_sub : public subscription::impl_base {
public:
  // -- member types -----------------------------------------------------------

  using output_type = output_type_t<Generator, Steps...>;

  // -- constructors, destructors, and assignment operators --------------------

  from_generator_sub(coordinator* ctx, observer<output_type> out,
                     const Generator& gen, const std::tuple<Steps...>& steps)
    : ctx_(ctx), out_(std::move(out)), gen_(gen), steps_(steps) {
    // nop
  }

  // -- callbacks for the generator / steps ------------------------------------

  bool on_next(const output_type& item) {
    buf_.push_back(item);
    return true;
  }

  void on_complete() {
    completed_ = true;
  }

  void on_error(const error& what) {
    completed_ = true;
    err_ = what;
  }

  // -- implementation of subscription -----------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void dispose() override {
    if (out_) {
      completed_ = true;
      buf_.clear();
      run_later();
    }
  }

  void request(size_t n) override {
    CAF_ASSERT(out_.valid());
    demand_ += n;
    run_later();
  }

private:
  void run_later() {
    if (!running_) {
      running_ = true;
      ctx_->delay_fn([strong_this = intrusive_ptr<from_generator_sub>{this}] {
        strong_this->do_run();
      });
    }
  }

  void do_run() {
    while (out_ && demand_ > 0) {
      while (buf_.empty() && !completed_)
        pull(demand_);
      if (!buf_.empty()) {
        --demand_;
        auto tmp = std::move(buf_.front());
        buf_.pop_front();
        out_.on_next(tmp);
      } else if (completed_) {
        fin();
        running_ = false;
        return;
      }
    }
    if (out_ && buf_.empty() && completed_)
      fin();
    running_ = false;
  }

  void fin() {
    if (!err_)
      out_.on_complete();
    else
      out_.on_error(err_);
    out_ = nullptr;
  }

  void pull(size_t n) {
    auto fn = [this, n](auto&... steps) { gen_.pull(n, steps..., *this); };
    std::apply(fn, steps_);
  }

  coordinator* ctx_;
  bool running_ = false;
  std::deque<output_type> buf_;
  bool completed_ = false;
  error err_;
  size_t demand_ = 0;
  observer<output_type> out_;
  Generator gen_;
  std::tuple<Steps...> steps_;
};

template <class Generator, class... Steps>
using from_generator_output_t =            //
  typename detail::tl_back_t<              //
    detail::type_list<Generator, Steps...> //
    >::output_type;

/// Converts a `Generator` to an @ref observable.
/// @note Depending on the `Generator`, this operator may turn *cold* if copying
///       the generator results in each copy emitting the exact same sequence of
///       values. However, we should treat it as *hot* by default.
template <class Generator, class... Steps>
class from_generator
  : public hot<from_generator_output_t<Generator, Steps...>> {
public:
  using output_type = from_generator_output_t<Generator, Steps...>;

  using super = hot<output_type>;

  from_generator(coordinator* ctx, Generator gen, std::tuple<Steps...> steps)
    : super(ctx), gen_(std::move(gen)), steps_(std::move(steps)) {
    // nop
  }

  // -- implementation of observable_impl<T> ---------------------------------

  disposable subscribe(observer<output_type> out) override {
    using impl_t = from_generator_sub<Generator, Steps...>;
    auto ptr = make_counted<impl_t>(super::ctx_, out, gen_, steps_);
    auto sub = subscription{std::move(ptr)};
    out.on_subscribe(sub);
    return std::move(sub).as_disposable();
  }

private:
  Generator gen_;
  std::tuple<Steps...> steps_;
};

} // namespace caf::flow::op
