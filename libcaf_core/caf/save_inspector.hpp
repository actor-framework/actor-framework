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

#include <utility>

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/inspector_access.hpp"
#include "caf/sec.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Base type for inspectors that save objects to some output sink. Deriving
/// from this class enables the inspector DSL.
/// @note The derived type still needs to provide an `object()` member function
///       for the DSL.
class CAF_CORE_EXPORT save_inspector {
public:
  // -- constants --------------------------------------------------------------

  /// Convenience constant to indicate success of a processing step.
  static constexpr bool ok = true;

  /// Convenience constant to indicate that a processing step failed and no
  /// further processing steps should take place.
  static constexpr bool stop = false;

  /// Enables dispatching on the inspector type.
  static constexpr bool is_loading = false;

  // -- legacy API -------------------------------------------------------------

  static constexpr bool reads_state = true;

  static constexpr bool writes_state = false;

  using result_type = bool;

  // -- constructors, destructors, and assignment operators --------------------

  virtual ~save_inspector();

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

  template <class T, class U>
  struct field_with_fallback_t {
    string_view field_name;
    T* val;
    U fallback;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto is_present = [this] { return *val != fallback; };
      auto get = [this] { return *val; };
      return inspector_access<T>::save_field(f, field_name, is_present, get);
    }

    template <class Predicate>
    field_with_fallback_t&& invariant(Predicate&&) && {
      return std::move(*this);
    }
  };

  template <class T>
  struct field_t {
    string_view field_name;
    T* val;

    template <class Inspector>
    bool operator()(Inspector& f) {
      return inspector_access<T>::save_field(f, field_name, *val);
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
    string_view field_name;
    Get get;
    U fallback;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto is_present = [this] { return get() != fallback; };
      return inspector_access<T>::save_field(f, field_name, is_present, get);
    }

    template <class Predicate>
    virt_field_with_fallback_t&& invariant(Predicate&&) && {
      return std::move(*this);
    }
  };

  template <class T, class Get>
  struct virt_field_t {
    string_view field_name;
    Get get;

    template <class Inspector>
    bool operator()(Inspector& f) {
      auto&& x = get();
      return inspector_access<T>::save_field(f, field_name,
                                             detail::as_mutable_ref(x));
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

  template <class Inspector, class SaveCallback>
  struct object_with_save_callback_t {
    string_view object_name;
    Inspector* f;
    SaveCallback save_callback;

    template <class... Fields>
    bool fields(Fields&&... fs) {
      using save_callback_result = decltype(save_callback());
        if (!(f->begin_object(object_name) && (fs(*f) && ...)))
          return false;
      if constexpr (std::is_same<save_callback_result, bool>::value) {
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

    auto pretty_name(string_view name) && {
      return object_t{name, f};
    }

    template <class F>
    object_with_save_callback_t&& on_load(F&&) && {
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
    object_t&& on_load(F&&) && {
      return std::move(*this);
    }

    template <class F>
    auto on_save(F fun) && {
      return object_with_save_callback_t<Inspector, F>{
        object_name,
        f,
        std::move(fun),
      };
    }
  };

  // -- factory functions ------------------------------------------------------

  template <class T>
  static auto field(string_view name, T& x) {
    return field_t<T>{name, &x};
  }

  template <class Get, class Set>
  static auto field(string_view name, Get get, Set&&) {
    using field_type = std::decay_t<decltype(get())>;
    return virt_field_t<field_type, Get>{name, get};
  }

protected:
  error err_;
};

} // namespace caf
