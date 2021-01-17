// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/inspector_access.hpp"
#include "caf/save_inspector.hpp"

namespace caf {

template <class Subtype>
class save_inspector_base : public save_inspector {
public:
  // -- member types -----------------------------------------------------------

  using super = save_inspector;

  // -- DSL entry points -------------------------------------------------------

  template <class T>
  constexpr auto object(T&) noexcept {
    return super::object_t<Subtype>{type_id_or_invalid<T>(),
                                    type_name_or_anonymous<T>(), dptr()};
  }

  constexpr auto virtual_object(string_view type_name) noexcept {
    return super::object_t<Subtype>{invalid_type_id, type_name, dptr()};
  }

  template <class T>
  bool list(const T& xs) {
    using value_type = typename T::value_type;
    auto size = xs.size();
    if (!dref().begin_sequence(size))
      return false;
    for (auto&& val : xs) {
      using found_type = std::decay_t<decltype(val)>;
      if constexpr (std::is_same<found_type, value_type>::value) {
        if (!detail::save(dref(), val))
          return false;
      } else {
        // Deals with atrocities like std::vector<bool>.
        auto tmp = static_cast<value_type>(val);
        if (!detail::save(dref(), tmp))
          return false;
      }
    }
    return dref().end_sequence();
  }

  template <class T>
  bool map(const T& xs) {
    if (!dref().begin_associative_array(xs.size()))
      return false;
    for (auto&& kvp : xs) {
      if (!(dref().begin_key_value_pair()       //
            && detail::save(dref(), kvp.first)  //
            && detail::save(dref(), kvp.second) //
            && dref().end_key_value_pair()))
        return false;
    }
    return dref().end_associative_array();
  }

  template <class T, size_t... Is>
  bool tuple(const T& xs, std::index_sequence<Is...>) {
    return dref().begin_tuple(sizeof...(Is))             //
           && (detail::save(dref(), get<Is>(xs)) && ...) //
           && dref().end_tuple();
  }

  template <class T>
  bool tuple(const T& xs) {
    return tuple(xs, std::make_index_sequence<std::tuple_size<T>::value>{});
  }

  template <class T, size_t N>
  bool tuple(T (&xs)[N]) {
    if (!dref().begin_tuple(N))
      return false;
    for (size_t index = 0; index < N; ++index)
      if (!detail::save(dref(), xs[index]))
        return false;
    return dref().end_tuple();
  }

  // -- dispatching to load/save functions -------------------------------------

  template <class T>
  [[nodiscard]] bool apply(const T& x) {
    return detail::save(dref(), x);
  }

  template <class Get, class Set>
  [[nodiscard]] bool apply(Get&& get, Set&&) {
    return detail::save(dref(), get());
  }

  // -- deprecated API: remove with CAF 0.19 -----------------------------------

  template <class T>
  [[deprecated("auto-conversion to underlying type is unsafe, add inspect")]] //
  std::enable_if_t<std::is_enum<T>::value, bool>
  opaque_value(T val) {
    return dref().value(static_cast<std::underlying_type_t<T>>(val));
  }

  template <class T>
  [[deprecated("use apply instead")]] bool apply_object(const T& x) {
    return apply(x);
  }

  template <class... Ts>
  [[deprecated("use apply instead")]] bool apply_objects(const Ts&... xs) {
    return (apply(xs) && ...);
  }

  template <class T>
  [[deprecated("use apply instead")]] bool apply_value(const T& x) {
    return apply(x);
  }

private:
  Subtype* dptr() {
    return static_cast<Subtype*>(this);
  }

  Subtype& dref() {
    return *static_cast<Subtype*>(this);
  }
};

} // namespace caf
