// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_access.hpp"
#include "caf/sec.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace caf::inspector {

/// Technology-independent serialization interface.
class CAF_CORE_EXPORT serializer {
public:
  // -- constants --------------------------------------------------------------

  /// Enables dispatching on the inspector type.
  static constexpr bool is_loading = false;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~serializer();

  // -- DSL types for regular fields -------------------------------------------

  template <class T, class U>
  struct field_with_fallback_t {
    std::string_view field_name;
    T* val;
    U fallback;

    bool operator()(serializer& f) {
      auto is_present = [this] { return *val != fallback; };
      auto get = [this]() -> decltype(auto) { return *val; };
      return detail::save_field(f, field_name, is_present, get);
    }

    template <class Predicate>
    field_with_fallback_t&& invariant(Predicate&&) && {
      return std::move(*this);
    }
  };

  template <class T>
  struct field_t {
    std::string_view field_name;
    T* val;

    bool operator()(serializer& f) {
      return detail::save_field(f, field_name, *val);
    }

    template <class U>
    auto fallback(U value) && {
      return field_with_fallback_t<T, U>{field_name, val, std::move(value)};
    }

    template <class Predicate>
    field_t&& invariant(Predicate&&) && {
      return std::move(*this);
    }
  };

  // -- DSL types for virtual fields (getter and setter access) ----------------

  template <class T, class Get, class U>
  struct virt_field_with_fallback_t {
    std::string_view field_name;
    Get get;
    U fallback;

    bool operator()(serializer& f) {
      auto is_present = [this] { return get() != fallback; };
      return detail::save_field(f, field_name, is_present, get);
    }

    template <class Predicate>
    virt_field_with_fallback_t&& invariant(Predicate&&) && {
      return std::move(*this);
    }
  };

  template <class T, class Get>
  struct virt_field_t {
    std::string_view field_name;
    Get get;

    bool operator()(serializer& f) {
      auto&& x = get();
      return detail::save_field(f, field_name, detail::as_mutable_ref(x));
    }

    template <class U>
    auto fallback(U value) && {
      return virt_field_with_fallback_t<T, Get, U>{
        field_name,
        std::move(get),
        std::move(value),
      };
    }

    template <class Predicate>
    virt_field_t&& invariant(Predicate&&) && {
      return std::move(*this);
    }
  };

  template <class T, class IsPresent, class Get>
  struct optional_virt_field_t {
    std::string_view field_name;
    IsPresent is_present;
    Get get;

    bool operator()(serializer& f) {
      return detail::save_field(f, field_name, is_present, get);
    }
  };

  // -- DSL type for the object ------------------------------------------------

  template <class SaveCallback>
  struct object_with_save_callback_t {
    type_id_t object_type;
    std::string_view object_name;
    serializer* f;
    SaveCallback save_callback;

    template <class... Fields>
    bool fields(Fields&&... fs) {
      using save_callback_result = decltype(save_callback());
      if (!(f->begin_object(object_type, object_name) && (fs(*f) && ...)))
        return false;
      if constexpr (std::is_same_v<save_callback_result, bool>) {
        if (!save_callback()) {
          f->set_error(sec::save_callback_failed);
          return false;
        }
      } else {
        if (auto err = save_callback()) {
          f->set_error(std::move(err));
          return false;
        }
      }
      return f->end_object();
    }

    auto pretty_name(std::string_view name) && {
      return object_t{name, f};
    }

    template <class F>
    object_with_save_callback_t&& on_load(F&&) && {
      return std::move(*this);
    }
  };

  struct object_t {
    type_id_t object_type;
    std::string_view object_name;
    serializer* f;

    template <class... Fields>
    bool fields(Fields&&... fs) {
      return f->begin_object(object_type, object_name) //
             && (fs(*f) && ...)                        //
             && f->end_object();
    }

    auto pretty_name(std::string_view name) && {
      return object_t{object_type, name, f};
    }

    template <class F>
    object_t&& on_load(F&&) && {
      return std::move(*this);
    }

    template <class F>
    auto on_save(F fun) && {
      return object_with_save_callback_t<F>{
        object_type,
        object_name,
        f,
        std::move(fun),
      };
    }
  };

  // -- factory functions ------------------------------------------------------

  template <class T>
  static auto field(std::string_view name, T& x) {
    static_assert(!std::is_const_v<T>);
    return field_t<T>{name, std::addressof(x)};
  }

  template <class Get, class Set>
  static auto field(std::string_view name, Get get, Set&&) {
    using field_type = std::decay_t<decltype(get())>;
    return virt_field_t<field_type, Get>{name, get};
  }

  template <class IsPresent, class Get, class... Ts>
  static auto
  field(std::string_view name, IsPresent is_present, Get get, Ts&&...) {
    using field_type = std::decay_t<decltype(get())>;
    return optional_virt_field_t<field_type, IsPresent, Get>{
      name,
      is_present,
      get,
    };
  }

  // -- DSL entry points -------------------------------------------------------

  template <class T>
  constexpr auto object(T&) noexcept {
    return object_t{type_id_or_invalid<T>(), type_name_or_anonymous<T>(), this};
  }

  constexpr auto virtual_object(std::string_view type_name) noexcept {
    return object_t{invalid_type_id, type_name, this};
  }

  template <class T>
  bool begin_object_t() {
    return begin_object(type_id_v<T>, caf::type_name_v<T>);
  }

  template <class T>
  bool list(const T& xs) {
    using value_type = typename T::value_type;
    auto size = xs.size();
    if (!begin_sequence(size))
      return false;
    for (auto&& val : xs) {
      using found_type = std::decay_t<decltype(val)>;
      if constexpr (std::is_same_v<found_type, value_type>) {
        if (!detail::save(this, val))
          return false;
      } else {
        // Deals with atrocities like std::vector<bool>.
        auto tmp = static_cast<value_type>(val);
        if (!detail::save(this, tmp))
          return false;
      }
    }
    return end_sequence();
  }

  template <class T>
  bool map(const T& xs) {
    if (!begin_associative_array(xs.size()))
      return false;
    for (auto&& kvp : xs) {
      if (!(begin_key_value_pair()            //
            && detail::save(this, kvp.first)  //
            && detail::save(this, kvp.second) //
            && end_key_value_pair()))
        return false;
    }
    return end_associative_array();
  }

  template <class T, size_t... Is>
  bool tuple(const T& xs, std::index_sequence<Is...>) {
    using std::get;
    return begin_tuple(sizeof...(Is))                  //
           && (detail::save(this, get<Is>(xs)) && ...) //
           && end_tuple();
  }

  template <class T>
  bool tuple(const T& xs) {
    return tuple(xs, std::make_index_sequence<std::tuple_size_v<T>>{});
  }

  template <class T, size_t N>
  bool tuple(T (&xs)[N]) {
    if (!begin_tuple(N))
      return false;
    for (size_t index = 0; index < N; ++index)
      if (!detail::save(this, xs[index]))
        return false;
    return end_tuple();
  }

  // -- dispatching to load/save functions -------------------------------------

  template <class T>
  [[nodiscard]] bool apply(const T& x) {
    return detail::save(this, x);
  }

  template <class Get, class Set>
  [[nodiscard]] bool apply(Get&& get, Set&&) {
    return detail::save(this, get());
  }

  // -- properties -------------------------------------------------------------

  virtual void set_error(error stop_reason) = 0;

  virtual error& get_error() noexcept = 0;

  template <class... Ts>
  void emplace_error(Ts&&... xs) {
    set_error(make_error(std::forward<Ts>(xs)...));
  }

  template <class... Ts>
  void field_invariant_check_failed(std::string msg) {
    emplace_error(sec::field_invariant_check_failed, std::move(msg));
  }

  template <class... Ts>
  void field_value_synchronization_failed(std::string msg) {
    emplace_error(sec::field_value_synchronization_failed, std::move(msg));
  }

  template <class... Ts>
  void invalid_field_type(std::string msg) {
    emplace_error(sec::invalid_field_type, std::move(msg));
  }

  /// Returns the actor system associated with this serializer if available.
  virtual caf::actor_system* sys() const noexcept = 0;

  /// Returns whether the serialization format is human-readable.
  virtual bool has_human_readable_format() const noexcept = 0;

  // -- interface functions ----------------------------------------------------

  /// Begins processing of an object. May save the type information to the
  /// underlying storage to allow a @ref deserializer to retrieve and check the
  /// type information for data formats that provide deserialization.
  virtual bool begin_object(type_id_t type, std::string_view name) = 0;

  /// Ends processing of an object.
  virtual bool end_object() = 0;

  virtual bool begin_field(std::string_view) = 0;

  virtual bool begin_field(std::string_view name, bool is_present) = 0;

  virtual bool begin_field(std::string_view name,
                           std::span<const type_id_t> types, size_t index)
    = 0;

  virtual bool begin_field(std::string_view name, bool is_present,
                           std::span<const type_id_t> types, size_t index)
    = 0;

  virtual bool end_field() = 0;

  /// Begins processing of a tuple.
  virtual bool begin_tuple(size_t size) = 0;

  /// Ends processing of a tuple.
  virtual bool end_tuple() = 0;

  /// Begins processing of a tuple with two elements, whereas the first element
  /// represents the key in an associative array.
  /// @note the default implementation calls `begin_tuple(2)`.
  virtual bool begin_key_value_pair();

  /// Ends processing of a key-value pair after both values were written.
  /// @note the default implementation calls `end_tuple()`.
  virtual bool end_key_value_pair();

  /// Begins processing of a sequence. Saves the size to the underlying storage.
  virtual bool begin_sequence(size_t size) = 0;

  /// Ends processing of a sequence.
  virtual bool end_sequence() = 0;

  /// Begins processing of an associative array (map).
  /// @note the default implementation calls `begin_sequence(size)`.
  virtual bool begin_associative_array(size_t size);

  /// Ends processing of an associative array (map).
  /// @note the default implementation calls `end_sequence()`.
  virtual bool end_associative_array();

  /// Adds `val` to the output.
  /// @param val A value for a builtin type.
  /// @returns `true` on success, `false` otherwise.
  virtual bool value(std::byte val) = 0;

  /// @copydoc value
  virtual bool value(bool val) = 0;

  /// @copydoc value
  template <std::integral T>
    requires(!std::is_same_v<T, bool>)
  bool value(T val) {
    return value(static_cast<detail::squashed_int_t<T>>(val));
  }

  /// @copydoc value
  virtual bool value(float val) = 0;

  /// @copydoc value
  virtual bool value(double val) = 0;

  /// @copydoc value
  virtual bool value(long double val) = 0;

  /// @copydoc value
  virtual bool value(std::string_view val) = 0;

  /// @copydoc value
  virtual bool value(std::u16string_view val) = 0;

  /// @copydoc value
  virtual bool value(std::u32string_view val) = 0;

  /// Adds `bytes` as raw byte block to the output.
  /// @param bytes The byte sequence.
  /// @returns `true` on success, `false` otherwise.
  virtual bool value(const_byte_span bytes) = 0;

  /// Adds the mail address of `ptr` to the output.
  /// @param ptr The actor pointer.
  /// @returns `true` on success, `false` otherwise.
  virtual bool value(const strong_actor_ptr& ptr);

  /// Adds the mail address of `ptr` to the output.
  /// @param ptr The actor pointer.
  /// @returns `true` on success, `false` otherwise.
  virtual bool value(const weak_actor_ptr& ptr);

  /// Adds each boolean in `vals` to the output. Derived classes can override
  /// this member function to pack the booleans, for evalample to avoid using
  /// one byte for each value in a binary output format.
  virtual bool list(const std::vector<bool>& vals);

private:
  virtual bool int_value(int8_t val) = 0;

  virtual bool int_value(uint8_t val) = 0;

  virtual bool int_value(int16_t val) = 0;

  virtual bool int_value(uint16_t val) = 0;

  virtual bool int_value(int32_t val) = 0;

  virtual bool int_value(uint32_t val) = 0;

  virtual bool int_value(int64_t val) = 0;

  virtual bool int_value(uint64_t val) = 0;
};

} // namespace caf::inspector
