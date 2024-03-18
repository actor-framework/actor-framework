// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/assert.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/scoped_coordinator.hpp"

#include <cstddef>
#include <memory>
#include <utility>

namespace caf::test::fixture {

/// A fixture for testing the flow API.
class CAF_TEST_EXPORT flow {
public:
  // -- member types -----------------------------------------------------------

  /// Represents the current state of an @ref observer.
  enum class observer_state {
    /// Indicates that no callbacks were called yet.
    idle,
    /// Indicates that on_subscribe was called.
    subscribed,
    /// Indicates that on_complete was called.
    completed,
    /// Indicates that on_error was called.
    aborted
  };

  /// @relates observer_state
  static std::string to_string(observer_state);

  /// An observer with minimal internal logic.
  template <class T>
  class passive_observer : public caf::flow::observer_impl_base<T> {
  public:
    explicit passive_observer(caf::flow::coordinator* parent)
      : parent_(parent) {
      // nop
    }

    // -- implementation of observer_impl<T> -----------------------------------

    caf::flow::coordinator* parent() const noexcept override {
      return parent_;
    }

    void on_complete() override {
      if (sub) {
        caf::flow::subscription tmp;
        tmp.swap(sub);
        tmp.cancel();
      }
      state = observer_state::completed;
    }

    void on_error(const error& what) override {
      sub.cancel();
      err = what;
      state = observer_state::aborted;
    }

    void on_subscribe(caf::flow::subscription new_sub) override {
      if (state == observer_state::idle) {
        CAF_ASSERT(!sub);
        sub = std::move(new_sub);
        state = observer_state::subscribed;
      } else {
        new_sub.cancel();
      }
    }

    void on_next(const T& item) override {
      if (!subscribed()) {
        auto what = "on_next called but observer is in state "
                    + to_string(state);
        CAF_RAISE_ERROR(std::logic_error, what.c_str());
      }
      buf.emplace_back(item);
    }

    // -- convenience functions ------------------------------------------------

    bool request(size_t demand) {
      if (sub) {
        sub.request(demand);
        return true;
      } else {
        return false;
      }
    }

    void unsubscribe() {
      if (sub) {
        sub.cancel();
        state = observer_state::idle;
      }
    }

    bool idle() const noexcept {
      return state == observer_state::idle;
    }

    bool subscribed() const noexcept {
      return state == observer_state::subscribed;
    }

    bool completed() const noexcept {
      return state == observer_state::completed;
    }

    bool aborted() const noexcept {
      return state == observer_state::aborted;
    }

    std::vector<T> sorted_buf() const {
      auto result = buf;
      std::sort(result.begin(), result.end());
      return result;
    }

    // -- member variables -----------------------------------------------------

    /// The subscription for requesting additional items.
    caf::flow::subscription sub;

    /// Default-constructed unless on_error was called.
    error err;

    /// Represents the current state of this observer.
    observer_state state = observer_state::idle;

    /// Stores all items received via `on_next`.
    std::vector<T> buf;

  private:
    caf::flow::coordinator* parent_;
  };

  /// A trivial observer that cancels its subscription, either immediately or
  /// when receiving the first item.
  template <class T>
  class canceling_observer : public caf::flow::observer_impl_base<T> {
  public:
    explicit canceling_observer(caf::flow::coordinator* parent,
                                bool accept_subscription)
      : accept_subscription(accept_subscription), parent_(parent) {
      // nop
    }

    caf::flow::coordinator* parent() const noexcept override {
      return parent_;
    }

    void on_next(const T&) override {
      ++on_next_calls;
      sub.cancel();
    }

    void on_error(const error&) override {
      ++on_error_calls;
      sub.release_later();
    }

    void on_complete() override {
      ++on_complete_calls;
      sub.release_later();
    }

    void on_subscribe(caf::flow::subscription sub) override {
      if (accept_subscription) {
        accept_subscription = false;
        sub.request(128);
        this->sub = std::move(sub);
        return;
      }
      sub.cancel();
    }

    int on_next_calls = 0;
    int on_error_calls = 0;
    int on_complete_calls = 0;
    bool accept_subscription = false;
    caf::flow::subscription sub;

  private:
    caf::flow::coordinator* parent_;
  };

  /// Similar to @ref passive_observer but automatically requests items until
  /// completed. Useful for writing unit tests.
  template <class T>
  class auto_observer : public passive_observer<T> {
  public:
    using super = passive_observer<T>;

    using super::super;

    void on_subscribe(caf::flow::subscription new_sub) override {
      if (this->state == observer_state::idle) {
        CAF_ASSERT(!this->sub);
        this->sub = std::move(new_sub);
        this->state = observer_state::subscribed;
        this->sub.request(64);
      } else {
        new_sub.cancel();
      }
    }

    void on_next(const T& item) override {
      super::on_next(item);
      if (this->sub)
        this->sub.request(1);
    }
  };

  // -- constructors, destructors, and assignment operators --------------------

  flow() {
    coordinator_ = caf::flow::make_scoped_coordinator();
  }

  // -- properties -------------------------------------------------------------

  /// Returns the coordinator used by this fixture.
  [[nodiscard]] caf::flow::coordinator* coordinator() noexcept {
    return coordinator_.get();
  }

  // -- factory functions ------------------------------------------------------

  /// Returns a new builder for creating observables.
  [[nodiscard]] auto make_observable() {
    return coordinator_->make_observable();
  }

  /// Returns a new passive observer.
  template <class T>
  intrusive_ptr<passive_observer<T>> make_passive_observer() {
    return coordinator()->add_child(std::in_place_type<passive_observer<T>>);
  }

  /// Returns a new auto observer.
  template <class T>
  intrusive_ptr<auto_observer<T>> make_auto_observer() {
    return coordinator()->add_child(std::in_place_type<auto_observer<T>>);
  }

  /// Returns a new canceling observer. The subscriber will either call `cancel`
  /// on its subscription immediately in `on_subscribe` or wait until the first
  /// call to `on_next` when setting `accept_first_subscription` to `true`.
  template <class T>
  auto make_canceling_observer(bool accept_first = false) {
    return make_counted<canceling_observer<T>>(accept_first);
  }

  /// Shortcut for creating an observable error via
  /// `make_observable<T>().fail<T>(args...)`. When passing an empty parameter
  /// pack, the error is constructed from sec::runtime_error.
  template <class T = int, class... Args>
  [[nodiscard]] auto obs_error(Args&&... args) {
    if constexpr (sizeof...(Args) == 0)
      return make_observable().fail<T>(error{sec::runtime_error});
    else
      return make_observable().fail<T>(error{std::forward<Args>(args)...});
  }

  /// Shortcut for `make_observable().range(init, num)`.
  template <class T>
  [[nodiscard]] auto range(T init, size_t num) {
    return make_observable().range(init, num);
  }

  /// Shortcut for `make_observable().just(arg)`
  template <class T>
  [[nodiscard]] auto just(T&& arg) {
    return make_observable().just(std::forward<T>(arg));
  }

  // -- conversion functions ---------------------------------------------------

  /// Builds a blueprint by calling `as_observable` on it.
  template <class Observable>
  [[nodiscard]] static auto build(Observable&& x) {
    return std::forward<Observable>(x).as_observable();
  }

  // -- control flow -----------------------------------------------------------

  /// Collects all values from an observable into a vector.
  template <class Observable>
  auto collect(Observable&& observable) {
    using value_type = caf::flow::output_type_t<std::decay_t<Observable>>;
    using list_type = std::vector<value_type>;
    using result_type = expected<list_type>;
    auto fin = std::make_shared<bool>(false);
    auto err = std::make_shared<error>();
    auto values = std::make_shared<list_type>();
    std::forward<Observable>(observable)
      .do_on_complete([fin] { *fin = true; })
      .do_on_error([fin, err](const error& e) {
        *fin = true;
        *err = std::move(e);
      })
      .for_each([values](const value_type& value) { //
        values->emplace_back(value);
      });
    run_flows();
    if (!*fin) {
      // TODO: use fail() on the current runnable instead.
      CAF_RAISE_ERROR(std::logic_error, "observable did not complete");
    }
    if (*err) {
      return result_type{std::move(*err)};
    }
    return result_type{std::move(*values)};
  }

  /// Runs all actions from the flows that are ready.
  void run_flows();

  /// Runs all actions from active flows that are ready or become ready before
  /// the relative timeout expires.
  template <class Rep, class Period>
  void run_flows(std::chrono::duration<Rep, Period> relative_timeout) {
    return run_flows(coordinator_->steady_time() + relative_timeout);
  }

  /// Runs all actions from active flows that are ready or become ready before
  /// the relative timeout expires.
  void run_flows(caf::flow::coordinator::steady_time_point timeout);

  /// Runs all actions from active flows that are ready or become ready before
  /// the absolute timeout expires.
  [[nodiscard]] caf::flow::coordinator* this_coordinator() {
    return coordinator_.get();
  }

private:
  caf::flow::scoped_coordinator_ptr coordinator_;
};

} // namespace caf::test::fixture
