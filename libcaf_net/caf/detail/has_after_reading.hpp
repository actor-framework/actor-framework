// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail {

template <class T, class LowerLayerPtr>
class has_after_reading {
private:
  template <class A, class B>
  static auto sfinae(A& up, B& ptr)
    -> decltype(up.after_reading(ptr), std::true_type{});

  template <class A>
  static std::false_type sfinae(A&, ...);

  using sfinae_result
    = decltype(sfinae(std::declval<T&>(), std::declval<LowerLayerPtr&>()));

public:
  static constexpr bool value = sfinae_result::value;
};

template <class T, class LowerLayerPtr>
constexpr bool has_after_reading_v = has_after_reading<T, LowerLayerPtr>::value;

} // namespace caf::detail
