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

#include "caf/error.hpp"
#include "caf/inspector_access.hpp"
#include "caf/sec.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Base type for inspectors that save objects to some output sink. Deriving
/// from this class enables the inspector DSL.
/// @note The derived type still needs to provide an `object()` member function
///       for the DSL.
class save_inspector {
public:
  // -- contants ---------------------------------------------------------------

  /// Convenience constant to indicate success of a processing step.
  static constexpr bool ok = true;

  /// Convenience constant to indicate that a processing step failed and no
  /// further processing steps should take place.
  static constexpr bool stop = false;

  /// Enables dispatching on the inspector type.
  static constexpr bool is_loading = false;

  /// A save inspector only reads the state of an object.
  static constexpr bool reads_state = true;

  /// A load inspector never modifies the state of an object.
  static constexpr bool writes_state = false;

  // -- DSL types for regular fields -------------------------------------------

  template <class T>
  struct field_t {
    string_view field_name;
    T* val;

    template <class Inspector>
    bool operator()(string_view, Inspector& f) {
      if constexpr (inspector_access_traits<T>::is_optional) {
        auto& ref = *val;
        using value_type = std::decay_t<decltype(*ref)>;
        if (ref) {
          return f.begin_field(field_name, true)                 //
                 && inspector_access<value_type>::apply(f, *ref) //
                 && f.end_field();
        } else {
          return f.begin_field(field_name, false) && f.end_field();
        }
      } else {
        return f.begin_field(field_name)              //
               && inspector_access<T>::apply(f, *val) //
               && f.end_field();
      }
    }

    template <class Unused>
    field_t& fallback(Unused&&) {
      return *this;
    }

    template <class Predicate>
    field_t invariant(Predicate&&) {
      return *this;
    }
  };

  // -- DSL types for virtual fields (getter and setter access) ----------------

  template <class T, class Get>
  struct virt_field_t {
    string_view field_name;
    Get get;

    template <class Inspector>
    bool operator()(string_view, Inspector& f) {
      if (!f.begin_field(field_name))
        return stop;
      auto&& value = get();
      using value_type = std::remove_reference_t<decltype(value)>;
      if constexpr (std::is_const<value_type>::value) {
        // Force a mutable reference, because the inspect API requires it. This
        // const_cast is always safe, because we never actually modify the
        // object.
        using mutable_ref = std::remove_const_t<value_type>&;
        if (!inspector_access<T>::apply(f, const_cast<mutable_ref>(value)))
          return stop;
      } else {
        if (!inspector_access<T>::apply(f, value))
          return stop;
      }
      return f.end_field();
    }

    template <class Unused>
    virt_field_t& fallback(Unused&&) {
      return *this;
    }

    template <class Predicate>
    virt_field_t invariant(Predicate&&) {
      return *this;
    }
  };

  template <class Inspector>
  struct object_t {
    string_view object_name;
    Inspector* f;

    template <class... Fields>
    bool fields(Fields&&... fs) {
      return f->begin_object(object_name) && (fs(object_name, *f) && ...)
             && f->end_object();
    }

    auto pretty_name(string_view name) && {
      return object_t{name, f};
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
};

} // namespace caf
