/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <type_traits>
#include <utility>

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/inspector_access.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Base type for inspectors that load objects from some input source. Deriving
/// from this class enables the inspector DSL.
/// @note The derived type still needs to provide an `object()` member function
///       for the DSL.
class CAF_CORE_EXPORT load_inspector {
public:
  // -- constants --------------------------------------------------------------

  /// Convenience constant to indicate success of a processing step.
  static constexpr bool ok = true;

  /// Convenience constant to indicate that a processing step failed and no
  /// further processing steps should take place.
  static constexpr bool stop = false;

  /// Enables dispatching on the inspector type.
  static constexpr bool is_loading = true;

  // -- legacy API -------------------------------------------------------------

  static constexpr bool reads_state = false;

  static constexpr bool writes_state = true;

  using result_type = bool;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~load_inspector();

  // -- properties -------------------------------------------------------------

  void set_error(error stop_reason) {
    err_ = std::move(stop_reason);
  }

  template <class... Ts>
  void emplace_error(Ts&&... xs) {
    err_ = make_error(xs...);
  }

  const error& get_error() const noexcept {
    return err_;
  }

  error&& move_error() noexcept {
    return std::move(err_);
  }

  // -- DSL types for regular fields -------------------------------------------

  template <class T, class U, class Predicate>
  struct field_with_invariant_and_fallback_t {
    string_view field_name;
    T* val;
    U fallback;
    Predicate predicate;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto reset = [this] { *val = std::move(fallback); };
      return inspector_access<T>::load_field(f, field_name, *val, predicate,
                                             detail::always_true, reset);
    }
  };

  template <class T, class U>
  struct field_with_fallback_t {
    string_view field_name;
    T* val;
    U fallback;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto reset = [this] { *val = std::move(fallback); };
      return inspector_access<T>::load_field(f, field_name, *val,
                                             detail::always_true,
                                             detail::always_true, reset);
    }

    template <class Predicate>
    auto invariant(Predicate predicate) && {
      return field_with_invariant_and_fallback_t<T, U, Predicate>{
        field_name,
        val,
        std::move(fallback),
        std::move(predicate),
      };
    }
  };

  template <class T, class Predicate>
  struct field_with_invariant_t {
    string_view field_name;
    T* val;
    Predicate predicate;

    template <class Inspector>
    bool operator()(Inspector& f) {
      return inspector_access<T>::load_field(f, field_name, *val, predicate,
                                             detail::always_true);
    }

    template <class U>
    auto fallback(U value) && {
      return field_with_invariant_and_fallback_t<T, U, Predicate>{
        field_name,
        val,
        std::move(value),
        std::move(predicate),
      };
    }
  };

  template <class T>
  struct field_t {
    string_view field_name;
    T* val;

    template <class Inspector>
    bool operator()(Inspector& f) {
      return inspector_access<T>::load_field(f, field_name, *val,
                                             detail::always_true,
                                             detail::always_true);
    }

    template <class U>
    auto fallback(U value) && {
      return field_with_fallback_t<T, U>{field_name, val, std::move(value)};
    }

    template <class Predicate>
    auto invariant(Predicate predicate) && {
      return field_with_invariant_t<T, Predicate>{
        field_name,
        val,
        std::move(predicate),
      };
    }
  };

  // -- DSL types for virtual fields (getter and setter access) ----------------

  template <class T, class Set, class U, class Predicate>
  struct virt_field_with_invariant_and_fallback_t {
    string_view field_name;
    Set set;
    U fallback;
    Predicate predicate;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto tmp = T{};
      auto sync = [this, &tmp] { return set(std::move(tmp)); };
      auto reset = [this] { set(std::move(fallback)); };
      return inspector_access<T>::load_field(f, field_name, tmp, predicate,
                                             sync, reset);
    }
  };

  template <class T, class Set, class U>
  struct virt_field_with_fallback_t {
    string_view field_name;
    Set set;
    U fallback;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto tmp = T{};
      auto sync = [this, &tmp] { return set(std::move(tmp)); };
      auto reset = [this] { set(std::move(fallback)); };
      return inspector_access<T>::load_field(f, field_name, tmp,
                                             detail::always_true, sync, reset);
    }

    template <class Predicate>
    auto invariant(Predicate predicate) && {
      return virt_field_with_invariant_and_fallback_t<T, Set, U, Predicate>{
        field_name,
        std::move(set),
        std::move(fallback),
        std::move(predicate),
      };
    }
  };

  template <class T, class Set, class Predicate>
  struct virt_field_with_invariant_t {
    string_view field_name;
    Set set;
    Predicate predicate;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto tmp = T{};
      auto sync = [this, &tmp] { return set(std::move(tmp)); };
      return inspector_access<T>::load_field(f, field_name, tmp, predicate,
                                             sync);
    }

    template <class U>
    auto fallback(U value) && {
      return virt_field_with_invariant_and_fallback_t<T, Set, U, Predicate>{
        field_name,
        std::move(set),
        std::move(value),
        std::move(predicate),
      };
    }
  };

  template <class T, class Set>
  struct virt_field_t {
    string_view field_name;
    Set set;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto tmp = T{};
      auto sync = [this, &tmp] { return set(std::move(tmp)); };
      return inspector_access<T>::load_field(f, field_name, tmp,
                                             detail::always_true, sync);
    }

    template <class U>
    auto fallback(U value) && {
      return virt_field_with_fallback_t<T, Set, U>{
        field_name,
        std::move(set),
        std::move(value),
      };
    }

    template <class Predicate>
    auto invariant(Predicate predicate) && {
      return virt_field_with_invariant_t<T, Set, Predicate>{
        field_name,
        std::move(set),
        std::move(predicate),
      };
    }
  };

  template <class T, class Reset, class Set>
  struct optional_virt_field_t {
    string_view field_name;
    Reset reset;
    Set set;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto tmp = T{};
      auto sync = [this, &tmp] { return set(std::move(tmp)); };
      return inspector_access<T>::load_field(f, field_name, tmp,
                                             detail::always_true, sync, reset);
    }
  };

  // -- DSL type for the object ------------------------------------------------

  template <class Inspector, class LoadCallback>
  struct object_with_load_callback_t {
    string_view object_name;
    Inspector* f;
    LoadCallback load_callback;

    template <class... Fields>
    bool fields(Fields&&... fs) {
      using load_callback_result = decltype(load_callback());
        if (!(f->begin_object(object_name) && (fs(*f) && ...)))
          return false;
      if constexpr (std::is_same<load_callback_result, bool>::value) {
        if (!load_callback()) {
          f->set_error(sec::load_callback_failed);
          return false;
        }
      } else {
        if (auto err = load_callback()) {
          f->set_error(std::move(err));
          return false;
        }
      }
      return f->end_object();
    }

    auto pretty_name(string_view name) && {
      return object_t{name, f};
    }

    template <class F>
    object_with_load_callback_t&& on_save(F&&) && {
      return std::move(*this);
    }
  };

  template <class Inspector>
  struct object_t {
    string_view object_name;
    Inspector* f;

    template <class... Fields>
    bool fields(Fields&&... fs) {
      return f->begin_object(object_name) && (fs(*f) && ...) && f->end_object();
    }

    auto pretty_name(string_view name) && {
      return object_t{name, f};
    }

    template <class F>
    object_t&& on_save(F&&) && {
      return std::move(*this);
    }

    template <class F>
    auto on_load(F fun) && {
      return object_with_load_callback_t<Inspector, F>{
        object_name,
        f,
        std::move(fun),
      };
    }
  };

  // -- factory functions ------------------------------------------------------

  template <class T>
  static auto field(string_view name, T& x) {
    static_assert(!std::is_const<T>::value);
    return field_t<T>{name, &x};
  }

  template <class Get, class Set>
  static auto field(string_view name, Get get, Set set) {
    using field_type = std::decay_t<decltype(get())>;
    return virt_field_t<field_type, Set>{name, set};
  }

  template <class IsPresent, class Get, class Reset, class Set>
  static auto
  field(string_view name, IsPresent&&, Get&& get, Reset reset, Set set) {
    using field_type = std::decay_t<decltype(get())>;
    return optional_virt_field_t<field_type, Reset, Set>{name, reset, set};
  }

protected:
  error err_;
};

} // namespace caf
