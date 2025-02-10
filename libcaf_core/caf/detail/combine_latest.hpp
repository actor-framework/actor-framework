// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/error.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/sec.hpp"
#include "caf/type_list.hpp"

#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace caf::detail {

/// Helper type to determine the intermediate type for the `combine_latest`
template <class, class>
struct combine_latest_intermediate_type_oracle;

template <size_t... Indexes, class... Types>
struct combine_latest_intermediate_type_oracle<
  std::integer_sequence<size_t, Indexes...>, type_list<Types...>> {
  using type = std::variant<
    std::pair<std::integral_constant<size_t, Indexes>, Types>...>;
};

/// Denotes the intermediate type for the `combine_latest` operator.
template <class IndexList, class TypeList>
using combine_latest_intermediate_type_t =
  typename combine_latest_intermediate_type_oracle<IndexList, TypeList>::type;

/// Allows the combine_latest operator to fail if an input observable completes
/// before emitting a value.
template <class T>
class fail_if_completed_before_first_value {
public:
  using input_type = T;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (!had_value_) {
      had_value_ = true;
    }
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    if (!had_value_) {
      next.on_error(make_error(sec::cannot_combine_empty_observables),
                    steps...);
    } else {
      next.on_complete(steps...);
    }
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }

private:
  bool had_value_ = false;
};

/// State for the `combine_latest` operator. Takes care of merging the inputs
/// into a single output.
template <class F, class... Ts>
class combine_latest_state {
public:
  template <class Fn>
  combine_latest_state(std::in_place_t, Fn&& f) : fn(std::forward<Fn>(f)) {
    // nop
  }

  using input_type = type_list<Ts...>;

  /// An index sequence for the inputs.
  using index_sequence = std::make_index_sequence<sizeof...(Ts)>;

  /// The intermediate type for the `combine_latest` operator, i.e., the type
  /// we map each input to before merging them.
  using intermediate_type
    = combine_latest_intermediate_type_t<index_sequence, input_type>;

  /// The return type of the user-defined merge function.
  using output_type = std::invoke_result_t<F, Ts...>;

  /// The tuple type storing the latest value for each input.
  using tuple_type = std::tuple<std::optional<Ts>...>;

  /// The user-defined merge function.
  F fn;

  /// Stores whether the observable is still in the cold boot phase, i.e., has
  /// not yet received a value for each input.
  bool cold_boot = true;

  /// Stores the latest value for each input.
  tuple_type values;

  /// Checks whether all inputs have produced at least one value.
  template <size_t... Indexes>
  bool init_done(std::integer_sequence<size_t, Indexes...>) {
    return (std::get<Indexes>(values).has_value() && ...);
  }

  /// Applies the user-defined function to the current values.
  template <size_t... Indexes>
  auto apply_fn(std::integer_sequence<size_t, Indexes...>) {
    return fn(*std::get<Indexes>(values)...);
  }

  /// Handles a new value from one of the inputs.
  template <size_t Index, class T>
  std::optional<output_type>
  on_next(std::integral_constant<size_t, Index>, const T& value) {
    auto& storage = std::get<Index>(values);
    // If the boot phase is over, each input produces a new output.
    if (!cold_boot) {
      *storage = value;
      return apply_fn(index_sequence{});
    }
    // Otherwise, we need to wait until all inputs have produced a value.
    if (storage.has_value()) {
      *storage = value;
      return std::nullopt;
    }
    storage.emplace(value);
    if (init_done(index_sequence{})) {
      cold_boot = false;
      return apply_fn(index_sequence{});
    }
    return std::nullopt;
  }

  /// Handles a new value from one of the inputs.
  std::optional<output_type> on_next(const intermediate_type& value) {
    return std::visit(
      [this](const auto& arg) { return this->on_next(arg.first, arg.second); },
      value);
  }

  /// Helper function for `combine_latest` to map the input of each observable
  /// to the intermediate type.
  template <size_t Index, class T>
  static auto map(std::integral_constant<size_t, Index>, T&& input) {
    using input_type = flow::output_type_t<std::decay_t<T>>;
    return std::forward<T>(input)
      .transform(fail_if_completed_before_first_value<input_type>{})
      .map([](const input_type& value) {
        using token = std::integral_constant<size_t, Index>;
        return intermediate_type{std::in_place_index_t<Index>{}, token{},
                                 value};
      });
  }
};

} // namespace caf::detail
