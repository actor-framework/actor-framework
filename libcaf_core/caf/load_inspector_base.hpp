// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <array>
#include <tuple>
#include <utility>

#include "caf/inspector_access.hpp"
#include "caf/load_inspector.hpp"
#include "caf/sec.hpp"

namespace caf {

template <class Subtype>
class load_inspector_base : public load_inspector {
public:
  // -- member types -----------------------------------------------------------

  using super = load_inspector;

  // -- DSL entry point --------------------------------------------------------

  template <class T>
  constexpr auto object(T&) noexcept {
    return super::object_t<Subtype>{type_id_or_invalid<T>(),
                                    type_name_or_anonymous<T>(), dptr()};
  }

  constexpr auto virtual_object(string_view type_name) noexcept {
    return super::object_t<Subtype>{invalid_type_id, type_name, dptr()};
  }

  template <class T>
  bool list(T& xs) {
    xs.clear();
    auto size = size_t{0};
    if (!dref().begin_sequence(size))
      return false;
    for (size_t i = 0; i < size; ++i) {
      auto val = typename T::value_type{};
      if (!detail::load(dref(), val))
        return false;
      xs.insert(xs.end(), std::move(val));
    }
    return dref().end_sequence();
  }

  template <class T>
  bool map(T& xs) {
    xs.clear();
    auto size = size_t{0};
    if (!dref().begin_associative_array(size))
      return false;
    for (size_t i = 0; i < size; ++i) {
      auto key = typename T::key_type{};
      auto val = typename T::mapped_type{};
      if (!(dref().begin_key_value_pair() //
            && detail::load(dref(), key)  //
            && detail::load(dref(), val)  //
            && dref().end_key_value_pair()))
        return false;
      // A multimap returns an iterator, a regular map returns a pair.
      auto emplace_result = xs.emplace(std::move(key), std::move(val));
      if constexpr (detail::is_pair<decltype(emplace_result)>::value) {
        if (!emplace_result.second) {
          super::emplace_error(sec::runtime_error, "multiple key definitions");
          return false;
        }
      }
    }
    return dref().end_associative_array();
  }

  template <class T, size_t... Is>
  bool tuple(T& xs, std::index_sequence<Is...>) {
    return dref().begin_tuple(sizeof...(Is))
           && (detail::load(dref(), get<Is>(xs)) && ...) //
           && dref().end_tuple();
  }

  template <class T>
  bool tuple(T& xs) {
    return tuple(xs, std::make_index_sequence<std::tuple_size<T>::value>{});
  }

  template <class T, size_t N>
  bool tuple(T (&xs)[N]) {
    if (!dref().begin_tuple(N))
      return false;
    for (size_t index = 0; index < N; ++index)
      if (!detail::load(dref(), xs[index]))
        return false;
    return dref().end_tuple();
  }

  // -- dispatching to load/load functions -------------------------------------

  template <class T>
  [[nodiscard]] bool apply(T& x) {
    static_assert(!std::is_const<T>::value);
    return detail::load(dref(), x);
  }

  /// Deserializes a primitive value with getter / setter access.
  template <class Get, class Set>
  [[nodiscard]] bool apply(Get&& get, Set&& set) {
    using value_type = std::decay_t<decltype(get())>;
    auto tmp = value_type{};
    using setter_result = decltype(set(std::move(tmp)));
    if constexpr (std::is_same<setter_result, bool>::value) {
      if (dref().apply(tmp)) {
        if (set(std::move(tmp))) {
          return true;
        } else {
          super::emplace_error(sec::save_callback_failed);
          return false;
        }
      } else {
        return false;
      }
    } else if constexpr (std::is_same<setter_result, void>::value) {
      if (dref().apply(tmp)) {
        set(std::move(tmp));
        return true;
      } else {
        return false;
      }
    } else {
      static_assert(std::is_convertible<setter_result, error>::value,
                    "a setter must return caf::error, bool or void");
      if (dref().apply(tmp)) {
        if (auto err = set(std::move(tmp)); !err) {
          return true;
        } else {
          super::set_error(std::move(err));
          return false;
        }
      } else {
        return false;
      }
    }
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
