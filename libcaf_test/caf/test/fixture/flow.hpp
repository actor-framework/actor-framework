// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"

#include <cstddef>
#include <memory>
#include <utility>

namespace caf::test::fixture {

/// A fixture for testing the flow API.
class CAF_CORE_EXPORT flow {
public:
  flow() {
    coordinator_ = caf::flow::make_scoped_coordinator();
  }

  /// Returns a new builder for creating observables.
  [[nodiscard]] auto make_observable() {
    return coordinator_->make_observable();
  }

  /// Shortcut for creating an observable error via
  /// `make_observable<T>().fail<T>(args...)`. When passing an empty parameter
  /// pack, the error is constructed from sec::runtime_error.
  template <class T = int, class... Args>
  [[nodiscard]] auto obs_error(Args&&... args) {
    if constexpr (sizeof...(Args) == 0)
      return make_observable().fail<T>(make_error(sec::runtime_error));
    else
      return make_observable().fail<T>(make_error(std::forward<Args>(args)...));
  }

  /// Shortcut for `make_observable<T>().range(init, num)`.
  template <class T>
  [[nodiscard]] auto range(T init, size_t num) {
    return make_observable().range(init, num);
  }

  /// Materializes an observable by calling `as_observable` on it.
  template <class Observable>
  [[nodiscard]] static auto mat(Observable&& x) {
    return std::forward<Observable>(x).as_observable();
  }

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

  /// Runs all flows created by this fixture.
  void run_flows();

  /// Returns the coordinator used by this fixture.
  [[nodiscard]] caf::flow::coordinator* this_coordinator() {
    return coordinator_.get();
  }

private:
  caf::flow::scoped_coordinator_ptr coordinator_;
};

} // namespace caf::test::fixture
