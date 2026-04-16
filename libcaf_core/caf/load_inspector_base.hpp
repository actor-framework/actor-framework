// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/inspector_access.hpp"
#include "caf/load_inspector.hpp"
#include "caf/placement_ptr.hpp"
#include "caf/sec.hpp"

#include <concepts>
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace caf {

template <class Subtype, class SubtypeInterface = void>
class load_inspector_base;

/// Adds entry points for the type inspection DSL.
template <class Subtype>
class load_inspector_base<Subtype, void> : public load_inspector {
public:
  // -- member types -----------------------------------------------------------

  using super = load_inspector;

  // -- DSL entry point --------------------------------------------------------

  template <class T>
  constexpr auto object(T&) noexcept {
    return super::object_t<Subtype>{type_id_or_invalid<T>(),
                                    type_name_or_anonymous<T>(), dptr()};
  }

  constexpr auto virtual_object(std::string_view type_name) noexcept {
    return super::object_t<Subtype>{invalid_type_id, type_name, dptr()};
  }

  template <class T>
  bool begin_object_t() {
    return dref().begin_object(type_id_v<T>, caf::type_name_v<T>);
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
      if constexpr (detail::is_pair<decltype(emplace_result)>) {
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
           && (detail::load(dref(), std::get<Is>(xs)) && ...) //
           && dref().end_tuple();
  }

  template <class T>
  bool tuple(T& xs) {
    return tuple(xs, std::make_index_sequence<std::tuple_size_v<T>>{});
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
    static_assert(!std::is_const_v<T>);
    return detail::load(dref(), x);
  }

  /// Deserializes a primitive value with getter / setter access.
  template <class Get, class Set>
  [[nodiscard]] bool apply(Get&& get, Set&& set) {
    using value_type = std::decay_t<decltype(get())>;
    auto tmp = value_type{};
    using setter_result = decltype(set(std::move(tmp)));
    if constexpr (std::is_same_v<setter_result, bool>) {
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
    } else if constexpr (std::is_same_v<setter_result, void>) {
      if (dref().apply(tmp)) {
        set(std::move(tmp));
        return true;
      } else {
        return false;
      }
    } else {
      static_assert(std::is_convertible_v<setter_result, error>,
                    "a setter must return caf::error, bool or void");
      if (dref().apply(tmp)) {
        if (auto err = set(std::move(tmp)); err.empty()) {
          return true;
        } else {
          dref().set_error(std::move(err));
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

/// Adds entry points for the type inspection DSL and dispatches common
/// operations to the implementation object.
template <class Subtype, class SubtypeInterface>
class load_inspector_base : public load_inspector_base<Subtype, void> {
public:
  void set_error(error stop_reason) final {
    impl_->set_error(std::move(stop_reason));
  }

  error& get_error() noexcept final {
    return impl_->get_error();
  }

  bool fetch_next_object_type(type_id_t& type) noexcept {
    return impl_->fetch_next_object_type(type);
  }

  bool begin_object(type_id_t type, std::string_view name) noexcept {
    return impl_->begin_object(type, name);
  }

  bool end_object() noexcept {
    return impl_->end_object();
  }

  bool begin_field(std::string_view name) noexcept {
    return impl_->begin_field(name);
  }

  bool begin_field(std::string_view name, bool& is_present) noexcept {
    return impl_->begin_field(name, is_present);
  }

  bool begin_field(std::string_view name, std::span<const type_id_t> types,
                   size_t& index) noexcept {
    return impl_->begin_field(name, types, index);
  }

  bool begin_field(std::string_view name, bool& is_present,
                   std::span<const type_id_t> types, size_t& index) noexcept {
    return impl_->begin_field(name, is_present, types, index);
  }

  bool end_field() {
    return impl_->end_field();
  }

  bool begin_tuple(size_t size) noexcept {
    return impl_->begin_tuple(size);
  }

  bool end_tuple() noexcept {
    return impl_->end_tuple();
  }

  bool begin_key_value_pair() noexcept {
    return impl_->begin_key_value_pair();
  }

  bool end_key_value_pair() noexcept {
    return impl_->end_key_value_pair();
  }

  bool begin_sequence(size_t& list_size) noexcept {
    return impl_->begin_sequence(list_size);
  }

  bool end_sequence() noexcept {
    return impl_->end_sequence();
  }

  bool begin_associative_array(size_t& size) noexcept {
    return impl_->begin_associative_array(size);
  }

  bool end_associative_array() noexcept {
    return impl_->end_associative_array();
  }

  bool value(bool& what) noexcept {
    return impl_->value(what);
  }

  bool value(std::byte& what) noexcept {
    return impl_->value(what);
  }

  bool value(uint8_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(int8_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(int16_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(uint16_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(int32_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(uint32_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(int64_t& what) noexcept {
    return impl_->value(what);
  }

  bool value(uint64_t& what) noexcept {
    return impl_->value(what);
  }

  template <std::integral T>
  bool value(T& what) noexcept {
    auto tmp = detail::squashed_int_t<T>{0};
    if (impl_->value(tmp)) {
      what = static_cast<T>(tmp);
      return true;
    } else {
      return false;
    }
  }

  template <std::floating_point T>
  bool value(T& what) noexcept {
    return impl_->value(what);
  }

  bool value(strong_actor_ptr& what) {
    return impl_->value(what);
  }

  bool value(weak_actor_ptr& what) {
    return impl_->value(what);
  }

  bool value(std::string& what) {
    return impl_->value(what);
  }

  bool value(std::u16string& what) {
    return impl_->value(what);
  }

  bool value(std::u32string& what) {
    return impl_->value(what);
  }

  bool value(byte_span what) noexcept {
    return impl_->value(what);
  }

  bool value(std::vector<bool>& x) {
    return impl_->value(x);
  }

  bool builtin_inspect(std::vector<bool>& x) {
    return value(x);
  }

  caf::actor_handle_codec* actor_handle_codec() {
    return impl_->actor_handle_codec();
  }

  SubtypeInterface& as_deserializer() noexcept {
    return *impl_;
  }

protected:
  explicit load_inspector_base(SubtypeInterface* impl) noexcept : impl_(impl) {
    // nop
  }

  /// Pointer to the implementation object.
  placement_ptr<SubtypeInterface> impl_;
};

} // namespace caf
