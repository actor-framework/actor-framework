// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/op/empty.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/make_counted.hpp"

#include <deque>
#include <memory>
#include <numeric>
#include <utility>
#include <variant>
#include <vector>

namespace caf::flow::op {

/// @relates merge
template <class T>
struct merge_input {
  /// The subscription to this input.
  subscription sub;

  /// Stores received items until the merge can forward them downstream.
  std::deque<T> buf;
};

template <class T>
class merge_sub : public subscription::impl_base {
public:
  // -- member types -----------------------------------------------------------

  using input_t = merge_input<T>;

  using input_key = size_t;

  using input_ptr = std::unique_ptr<input_t>;

  using input_map = unordered_flat_map<input_key, input_ptr>;

  // -- constructors, destructors, and assignment operators --------------------

  merge_sub(coordinator* ctx, observer<T> out)
    : ctx_(ctx), out_(std::move(out)) {
    // nop
  }

  // -- input management -------------------------------------------------------

  void subscribe_to(observable<T> what) {
    auto key = next_key_++;
    inputs_.container().emplace_back(key, std::make_unique<input_t>());
    using fwd_impl = forwarder<T, merge_sub, size_t>;
    auto fwd = make_counted<fwd_impl>(this, key);
    what.subscribe(fwd->as_observer());
  }

  void subscribe_to(observable<observable<T>> what) {
    auto key = next_key_++;
    auto& vec = inputs_.container();
    vec.emplace_back(key, std::make_unique<input_t>());
    using fwd_impl = forwarder<observable<T>, merge_sub, size_t>;
    auto fwd = make_counted<fwd_impl>(this, key);
    what.subscribe(fwd->as_observer());
  }

  // -- callbacks for the forwarders -------------------------------------------

  void fwd_on_subscribe(input_key key, subscription sub) {
    CAF_LOG_TRACE(CAF_ARG(key));
    if (auto ptr = get(key); ptr && !ptr->sub && out_) {
      sub.request(max_pending_);
      ptr->sub = std::move(sub);
    } else {
      sub.dispose();
    }
  }

  void fwd_on_complete(input_key key) {
    CAF_LOG_TRACE(CAF_ARG(key));
    if (auto i = inputs_.find(key); i != inputs_.end()) {
      if (i->second->buf.empty()) {
        inputs_.erase(i);
        run_later();
      } else {
        i->second->sub = nullptr;
      }
    }
  }

  void fwd_on_error(input_key key, const error& what) {
    CAF_LOG_TRACE(CAF_ARG(key) << CAF_ARG(what));
    if (!err_) {
      err_ = what;
      if (!flags_.delay_error) {
        auto i = inputs_.begin();
        while (i != inputs_.end()) {
          auto& input = *i->second;
          if (auto& sub = input.sub) {
            sub.dispose();
            sub = nullptr;
          }
          if (input.buf.empty())
            i = inputs_.erase(i);
          else
            ++i;
        }
      }
    }
    fwd_on_complete(key);
  }

  void fwd_on_next(input_key key, const T& item) {
    CAF_LOG_TRACE(CAF_ARG(key) << CAF_ARG(item));
    if (auto ptr = get(key)) {
      if (!flags_.running && demand_ > 0) {
        CAF_ASSERT(out_.valid());
        --demand_;
        out_.on_next(item);
        ptr->sub.request(1);
      } else {
        ptr->buf.push_back(item);
      }
    }
  }

  void fwd_on_next(input_key key, const observable<T>& item) {
    CAF_LOG_TRACE(CAF_ARG(key) << CAF_ARG(item));
    if (auto ptr = get(key))
      subscribe_to(item);
    // Note: we need to double-check that the key still exists here, because
    //       subscribe_on may result in an error (that nukes all inputs).
    if (auto ptr = get(key))
      ptr->sub.request(1);
  }

  // -- implementation of subscription_impl ------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void dispose() override {
    if (out_) {
      for (auto& kvp : inputs_)
        if (auto& sub = kvp.second->sub)
          sub.dispose();
      inputs_.clear();
      run_later();
    }
  }

  void request(size_t n) override {
    CAF_ASSERT(out_.valid());
    demand_ += n;
    if (demand_ == n)
      run_later();
  }

  size_t buffered() const noexcept {
    return std::accumulate(inputs_.begin(), inputs_.end(), size_t{0},
                           [](size_t n, auto& kvp) {
                             return n + kvp.second->buf.size();
                           });
  }

private:
  void run_later() {
    if (!flags_.running) {
      flags_.running = true;
      ctx_->delay_fn([strong_this = intrusive_ptr<merge_sub>{this}] {
        strong_this->do_run();
      });
    }
  }

  auto next_input() {
    CAF_ASSERT(!inputs_.empty());
    auto has_items_at = [this](size_t pos) {
      auto& vec = inputs_.container();
      return !vec[pos].second->buf.empty();
    };
    auto start = pos_ % inputs_.size();
    pos_ = (pos_ + 1) % inputs_.size();
    if (has_items_at(start))
      return inputs_.begin() + start;
    while (pos_ != start) {
      auto p = pos_;
      pos_ = (pos_ + 1) % inputs_.size();
      if (has_items_at(p))
        return inputs_.begin() + p;
    }
    return inputs_.end();
  }

  void do_run() {
    while (out_ && demand_ > 0 && !inputs_.empty()) {
      if (auto i = next_input(); i != inputs_.end()) {
        --demand_;
        auto& buf = i->second->buf;
        auto tmp = std::move(buf.front());
        buf.pop_front();
        if (auto& sub = i->second->sub) {
          sub.request(1);
        } else if (buf.empty()) {
          inputs_.erase(i);
        }
        out_.on_next(tmp);
      } else {
        break;
      }
    }
    if (out_ && inputs_.empty())
      fin();
    flags_.running = false;
  }

  void fin() {
    if (!inputs_.empty()) {
      for (auto& kvp : inputs_)
        if (auto& sub = kvp.second->sub)
          sub.dispose();
      inputs_.clear();
    }
    if (!err_) {
      out_.on_complete();
    } else {
      out_.on_error(err_);
    }
    out_ = nullptr;
  }

  /// Selects an input object by key or returns null.
  input_t* get(input_key key) {
    if (auto i = inputs_.find(key); i != inputs_.end())
      return i->second.get();
    else
      return nullptr;
  }

  /// Groups various Boolean flags.
  struct flags_t {
    /// Configures whether an error immediately aborts the merging or not.
    bool delay_error : 1;

    /// Stores whether the merge is currently executing do_run.
    bool running : 1;

    flags_t() : delay_error(false), running(false) {
      // nop
    }
  };

  /// Stores the context (coordinator) that runs this flow.
  coordinator* ctx_;

  /// Stores the first error that occurred on any input.
  error err_;

  /// Fine-tunes the behavior of the merge.
  flags_t flags_;

  /// Stores our current demand for items from the subscriber.
  size_t demand_ = 0;

  /// Stores a handle to the subscriber.
  observer<T> out_;

  /// Stores the last round-robin read position.
  size_t pos_ = 0;

  /// Associates inputs with ascending keys.
  input_map inputs_;

  /// Stores the key for the next input.
  size_t next_key_ = 0;

  /// Configures how many items we buffer per input.
  size_t max_pending_ = defaults::flow::buffer_size;
};

template <class T>
class merge : public cold<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = cold<T>;

  using input_type = std::variant<observable<T>, observable<observable<T>>>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts, class... Inputs>
  explicit merge(coordinator* ctx, Inputs&&... inputs) : super(ctx) {
    (add(std::forward<Inputs>(inputs)), ...);
  }

  // -- properties -------------------------------------------------------------

  size_t inputs() const noexcept {
    return inputs_.size();
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    if (inputs() == 0) {
      auto ptr = make_counted<empty<T>>(super::ctx_);
      return ptr->subscribe(std::move(out));
    } else {
      auto sub = make_counted<merge_sub<T>>(super::ctx_, out);
      for (auto& input : inputs_)
        std::visit([&sub](auto& in) { sub->subscribe_to(in); }, input);
      out.on_subscribe(subscription{sub});
      return sub->as_disposable();
    }
  }

private:
  template <class Input>
  void add(Input&& x) {
    using input_t = std::decay_t<Input>;
    if constexpr (detail::is_iterable_v<input_t>) {
      for (auto& in : x)
        add(in);
    } else {
      static_assert(is_observable_v<input_t>);
      inputs_.emplace_back(std::forward<Input>(x).as_observable());
    }
  }

  std::vector<input_type> inputs_;
};

} // namespace caf::flow::op
