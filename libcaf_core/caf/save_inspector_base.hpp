// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/inspector_access.hpp"
#include "caf/placement_ptr.hpp"
#include "caf/save_inspector.hpp"

#include <concepts>
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace caf {

template <class Subtype, class SubtypeInterface = void>
class save_inspector_base;

template <class Subtype>
class save_inspector_base<Subtype, void> : public save_inspector {
public:
  // -- member types -----------------------------------------------------------

  using super = save_inspector;

  // -- DSL entry points -------------------------------------------------------

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
  bool list(const T& xs) {
    using value_type = typename T::value_type;
    auto size = xs.size();
    if (!dref().begin_sequence(size))
      return false;
    for (auto&& val : xs) {
      using found_type = std::decay_t<decltype(val)>;
      if constexpr (std::is_same_v<found_type, value_type>) {
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
    using std::get;
    return dref().begin_tuple(sizeof...(Is))             //
           && (detail::save(dref(), get<Is>(xs)) && ...) //
           && dref().end_tuple();
  }

  template <class T>
  bool tuple(const T& xs) {
    return tuple(xs, std::make_index_sequence<std::tuple_size_v<T>>{});
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

private:
  Subtype* dptr() {
    return static_cast<Subtype*>(this);
  }

  Subtype& dref() {
    return *static_cast<Subtype*>(this);
  }
};

template <class Subtype, class SubtypeInterface>
class save_inspector_base : public save_inspector_base<Subtype, void> {
public:
  void set_error(error stop_reason) final {
    impl_->set_error(std::move(stop_reason));
  }

  error& get_error() noexcept final {
    return impl_->get_error();
  }

  bool begin_object(type_id_t id, std::string_view name) noexcept {
    return impl_->begin_object(id, name);
  }

  bool end_object() {
    return impl_->end_object();
  }

  bool begin_field(std::string_view type_name) noexcept {
    return impl_->begin_field(type_name);
  }

  bool begin_field(std::string_view type_name, bool is_present) {
    return impl_->begin_field(type_name, is_present);
  }

  bool begin_field(std::string_view type_name, std::span<const type_id_t> types,
                   size_t index) {
    return impl_->begin_field(type_name, types, index);
  }

  bool begin_field(std::string_view type_name, bool is_present,
                   std::span<const type_id_t> types, size_t index) {
    return impl_->begin_field(type_name, is_present, types, index);
  }

  bool end_field() {
    return impl_->end_field();
  }

  bool begin_tuple(size_t size) {
    return impl_->begin_tuple(size);
  }

  bool end_tuple() {
    return impl_->end_tuple();
  }

  bool begin_key_value_pair() {
    return impl_->begin_key_value_pair();
  }

  bool end_key_value_pair() {
    return impl_->end_key_value_pair();
  }

  bool begin_sequence(size_t list_size) {
    return impl_->begin_sequence(list_size);
  }

  bool end_sequence() {
    return impl_->end_sequence();
  }

  bool begin_associative_array(size_t size) {
    return impl_->begin_associative_array(size);
  }

  bool end_associative_array() {
    return impl_->end_associative_array();
  }

  bool value(std::byte what) {
    return impl_->value(what);
  }

  bool value(bool what) {
    return impl_->value(what);
  }

  template <std::integral T>
    requires(!std::same_as<T, bool>)
  bool value(T what) {
    return impl_->value(static_cast<detail::squashed_int_t<T>>(what));
  }

  template <std::floating_point T>
  bool value(T what) {
    return impl_->value(what);
  }

  bool value(const strong_actor_ptr& what) {
    return impl_->value(what);
  }

  bool value(std::string_view what) {
    return impl_->value(what);
  }

  bool value(const std::u16string& what) {
    return impl_->value(what);
  }

  bool value(const std::u32string& what) {
    return impl_->value(what);
  }

  bool value(const_byte_span what) {
    return impl_->value(what);
  }

  bool value(const std::vector<bool>& x) {
    return impl_->value(x);
  }

  bool builtin_inspect(const std::vector<bool>& x) {
    return value(x);
  }

  caf::actor_handle_codec* actor_handle_codec() {
    return impl_->actor_handle_codec();
  }

  SubtypeInterface& as_serializer() noexcept {
    return *impl_;
  }

protected:
  explicit save_inspector_base(SubtypeInterface* impl) noexcept : impl_(impl) {
    // nop
  }

  /// Pointer to the implementation object.
  placement_ptr<SubtypeInterface> impl_;
};

} // namespace caf
