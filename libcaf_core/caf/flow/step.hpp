// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/observable_state.hpp"
#include "caf/flow/observer.hpp"

#include <algorithm>
#include <type_traits>

namespace caf::flow {

template <class T>
struct identity_step {
  using input_type = T;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T>
struct limit_step {
  size_t remaining;

  using input_type = T;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (remaining > 0) {
      if (next.on_next(item, steps...)) {
        if (--remaining > 0) {
          return true;
        } else {
          next.on_complete(steps...);
          return false;
        }
      }
    }
    return false;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class Predicate>
struct filter_step {
  using trait = detail::get_callable_trait_t<Predicate>;

  static_assert(std::is_convertible_v<typename trait::result_type, bool>,
                "predicates must return a boolean value");

  static_assert(trait::num_args == 1,
                "predicates must take exactly one argument");

  using input_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;

  using output_type = input_type;

  Predicate predicate;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (predicate(item))
      return next.on_next(item, steps...);
    else
      return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class Fn>
struct map_step {
  using trait = detail::get_callable_trait_t<Fn>;

  static_assert(!std::is_same_v<typename trait::result_type, void>,
                "map functions may not return void");

  static_assert(trait::num_args == 1,
                "map functions must take exactly one argument");

  using input_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;

  using output_type = std::decay_t<typename trait::result_type>;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(fn(item), steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class Fn>
struct flat_map_optional_step {
  using trait = detail::get_callable_trait_t<Fn>;

  static_assert(!std::is_same_v<typename trait::result_type, void>,
                "flat_map_optional functions may not return void");

  static_assert(trait::num_args == 1,
                "flat_map_optional functions must take exactly one argument");

  using input_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;

  using intermediate_type = std::decay_t<typename trait::result_type>;

  using output_type = typename intermediate_type::value_type;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (auto val = fn(item))
      return next.on_next(*val, steps...);
    else
      return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T, class Fn>
struct do_on_complete_step {
  using input_type = T;

  using output_type = T;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    fn();
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T, class Fn>
struct do_on_error_step {
  using input_type = T;

  using output_type = T;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    fn(what);
    next.on_error(what, steps...);
  }
};

template <class T, class Fn>
struct do_finally_step {
  using input_type = T;

  using output_type = T;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    fn();
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    fn();
    next.on_error(what, steps...);
  }
};

/// Catches errors by converting them into complete events instead.
template <class T>
struct on_error_complete_step {
  using input_type = T;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error&, Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }
};

/// Wraps logic for pushing data to multiple observers with broadcast semantics,
/// i.e., all observers see the same items at the same time and the flow adjusts
/// to the slowest observer. This step may only be used as terminal step.
template <class T>
class broadcast_step {
public:
  // -- member types -----------------------------------------------------------

  using output_type = T;

  using observer_impl_t = observer_impl<output_type>;

  struct output_t {
    size_t demand;
    observer<output_type> sink;
  };

  // -- constructors, destructors, and assignment operators --------------------

  broadcast_step() {
    // Reserve some buffer space in order to avoid frequent re-allocations while
    // warming up.
    buf_.reserve(32);
  }

  // -- properties -------------------------------------------------------------

  size_t min_demand() const noexcept {
    if (!outputs_.empty()) {
      auto i = outputs_.begin();
      auto init = (*i++).demand;
      return std::accumulate(i, outputs_.end(), init,
                             [](size_t x, const output_t& y) {
                               return std::min(x, y.demand);
                             });
    } else {
      return 0;
    }
  }

  size_t max_demand() const noexcept {
    if (!outputs_.empty()) {
      auto i = outputs_.begin();
      auto init = (*i++).demand;
      return std::accumulate(i, outputs_.end(), init,
                             [](size_t x, const output_t& y) {
                               return std::max(x, y.demand);
                             });
    } else {
      return 0;
    }
  }

  /// Returns how many items are currently buffered at this step.
  size_t buffered() const noexcept {
    return buf_.size();
  }

  /// Returns the number of current observers.
  size_t num_observers() const noexcept {
    return outputs_.size();
  }

  /// Convenience function for calling `is_active(state())`;
  bool active() const noexcept {
    return is_active(state_);
  }

  /// Queries whether the current state is `observable_state::completing`.
  bool completing() const noexcept {
    return state_ == observable_state::completing;
  }

  /// Convenience function for calling `is_final(state())`;
  bool finalized() const noexcept {
    return is_final(state_);
  }

  /// Returns the current state.
  observable_state state() const noexcept {
    return state_;
  }

  const error& err() const noexcept {
    return err_;
  }

  void err(error x) {
    err_ = std::move(x);
  }

  // -- demand management ------------------------------------------------------

  size_t next_demand() {
    auto have = buf_.size() + in_flight_;
    auto want = max_demand();
    if (want > have) {
      auto delta = want - have;
      in_flight_ += delta;
      return delta;
    } else {
      return 0;
    }
  }

  // -- callbacks for the parent -----------------------------------------------

  /// Tries to add a new observer.
  bool add(observer<output_type> sink) {
    if (is_active(state_)) {
      outputs_.emplace_back(output_t{0, std::move(sink)});
      return true;
    } else if (err_) {
      sink.on_error(err_);
      return false;
    } else {
      sink.on_error(make_error(sec::disposed));
      return false;
    }
  }

  /// Tries to add a new observer and returns `parent->do_subscribe(sink)` on
  /// success or a default-constructed @ref disposable otherwise.
  template <class Parent>
  disposable add(Parent* parent, observer<output_type> sink) {
    if (add(sink)) {
      return parent->do_subscribe(sink);
    } else {
      return disposable{};
    }
  }

  /// Requests `n` more items for `sink`.
  /// @returns New demand to signal upstream or 0.
  /// @note Calls @ref push.
  size_t on_request(observer_impl_t* sink, size_t n) {
    if (auto i = find(sink); i != outputs_.end()) {
      i->demand += n;
      push();
      return next_demand();
    } else {
      return 0;
    }
  }

  /// Requests `n` more items for `sink`.
  /// @note Calls @ref push and may call `sub.request(n)`.
  void on_request(subscription& sub, observer_impl_t* sink, size_t n) {
    if (auto new_demand = on_request(sink, n); new_demand > 0 && sub)
      sub.request(new_demand);
  }

  /// Removes `sink` from the observer set.
  /// @returns New demand to signal upstream or 0.
  /// @note Calls @ref push.
  size_t on_cancel(observer_impl_t* sink) {
    if (auto i = find(sink); i != outputs_.end()) {
      outputs_.erase(i);
      push();
      return next_demand();
    } else {
      return 0;
    }
  }

  /// Requests `n` more items for `sink`.
  /// @note Calls @ref push and may call `sub.request(n)`.
  void on_cancel(subscription& sub, observer_impl_t* sink) {
    if (auto new_demand = on_cancel(sink); new_demand > 0 && sub)
      sub.request(new_demand);
  }

  /// Tries to deliver items from the buffer to the observers.
  void push() {
    // Must not be re-entered. Any on_request call must use the event loop.
    CAF_ASSERT(!pushing_);
    CAF_DEBUG_STMT(pushing_ = true);
    // Sanity checking.
    if (outputs_.empty())
      return;
    // Push data downstream and adjust demand on each path.
    if (auto n = std::min(min_demand(), buf_.size()); n > 0) {
      auto items = span<output_type>{buf_.data(), n};
      for (auto& out : outputs_) {
        out.demand -= n;
        out.sink.on_next(items);
      }
      buf_.erase(buf_.begin(), buf_.begin() + n);
    }
    if (state_ == observable_state::completing && buf_.empty()) {
      if (!err_) {
        for (auto& out : outputs_)
          out.sink.on_complete();
        state_ = observable_state::completed;
      } else {
        for (auto& out : outputs_)
          out.sink.on_error(err_);
        state_ = observable_state::aborted;
      }
    }
    CAF_DEBUG_STMT(pushing_ = false);
  }

  /// Checks whether the broadcaster currently has no pending data.
  bool idle() {
    return buf_.empty();
  }

  /// Calls `on_complete` on all observers and drops any pending data.
  void close() {
    buf_.clear();
    if (!err_) {
      for (auto& out : outputs_)
        out.sink.on_complete();
      state_ = observable_state::completed;
    } else {
      for (auto& out : outputs_)
        out.sink.on_error(err_);
      state_ = observable_state::aborted;
    }
    outputs_.clear();
  }

  /// Calls `on_error` on all observers and drops any pending data.
  void abort(const error& reason) {
    err_ = reason;
    close();
  }

  // -- callbacks for steps ----------------------------------------------------

  bool on_next(const output_type& item) {
    // Note: we may receive more data than what we have requested.
    if (in_flight_ > 0)
      --in_flight_;
    buf_.emplace_back(item);
    return true;
  }

  void on_next(span<const output_type> items) {
    // Note: we may receive more data than what we have requested.
    if (in_flight_ >= items.size())
      in_flight_ -= items.size();
    else
      in_flight_ = 0;
    buf_.insert(buf_.end(), items.begin(), items.end());
  }

  void fin() {
    if (is_active(state_)) {
      if (idle()) {
        close();
      } else {
        state_ = observable_state::completing;
      }
    }
  }

  void on_complete() {
    fin();
  }

  void on_error(const error& what) {
    err_ = what;
    fin();
  }

  // -- callbacks for the parent -----------------------------------------------

  void dispose() {
    on_complete();
  }

  /// Tries to set the state from `idle` to `running`.
  bool start() {
    if (state_ == observable_state::idle) {
      state_ = observable_state::running;
      return true;
    } else {
      return false;
    }
  }

  /// Tries to set the state from `idle` to `running`. On success, requests
  /// items on `sub` if there is already demand. Calls `sub.cancel()` when
  /// returning `false`.
  bool start(subscription& sub) {
    if (start()) {
      if (auto n = next_demand(); n > 0)
        sub.request(n);
      return true;
    } else {
      sub.cancel();
      return false;
    }
  }

private:
  typename std::vector<output_t>::iterator
  find(observer_impl<output_type>* sink) {
    auto e = outputs_.end();
    auto pred = [sink](const output_t& out) { return out.sink.ptr() == sink; };
    return std::find_if(outputs_.begin(), e, pred);
  }

  /// Buffers outbound items until we can ship them.
  std::vector<output_type> buf_;

  /// Keeps track of how many items have been requested but did not arrive yet.
  size_t in_flight_ = 0;

  /// Stores handles to the observer plus their demand.
  std::vector<output_t> outputs_;

  /// Keeps track of our current state.
  observable_state state_ = observable_state::idle;

  /// Stores the on_error argument.
  error err_;

#ifdef CAF_ENABLE_RUNTIME_CHECKS
  /// Protect against re-entering `push`.
  bool pushing_ = false;
#endif
};

/// Utility for the observables that use one or more steps.
template <class... Steps>
struct steps_output_oracle;

template <class Step>
struct steps_output_oracle<Step> {
  using type = typename Step::output_type;
};

template <class Step1, class Step2, class... Steps>
struct steps_output_oracle<Step1, Step2, Steps...>
  : steps_output_oracle<Step2, Steps...> {};

template <class... Steps>
using steps_output_type_t = typename steps_output_oracle<Steps...>::type;

} // namespace caf::flow
