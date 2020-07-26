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

#include "caf/error.hpp"
#include "caf/inspector_access.hpp"
#include "caf/sec.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Base type for inspectors that load objects from some input source. Deriving
/// from this class enables the inspector DSL.
/// @note The derived type still needs to provide an `object()` member function
///       for the DSL.
class load_inspector {
public:
  // -- contants ---------------------------------------------------------------

  /// Convenience constant to indicate success of a processing step.
  static constexpr bool ok = true;

  /// Convenience constant to indicate that a processing step failed and no
  /// further processing steps should take place.
  static constexpr bool stop = false;

  /// Enables dispatching on the inspector type.
  static constexpr bool is_loading = true;

  /// A load inspector never reads the state of an object.
  static constexpr bool reads_state = false;

  /// A load inspector overrides the state of an object.
  static constexpr bool writes_state = true;

  // -- error management -------------------------------------------------------

  template <class Inspector>
  static void set_invariant_check_error(Inspector& f, string_view field_name,
                                        string_view object_name) {
    std::string msg = "invalid argument to field ";
    msg.insert(msg.end(), field_name.begin(), field_name.end());
    msg += " for object of type ";
    msg.insert(msg.end(), object_name.begin(), object_name.end());
    msg += ": invariant check failed";
    f.set_error(make_error(caf::sec::invalid_argument, std::move(msg)));
  }

  template <class Inspector>
  static void set_field_store_error(Inspector& f, string_view field_name,
                                    string_view object_name) {
    std::string msg = "invalid argument to field ";
    msg.insert(msg.end(), field_name.begin(), field_name.end());
    msg += " for object of type ";
    msg.insert(msg.end(), object_name.begin(), object_name.end());
    msg += ": setter returned false";
    f.set_error(make_error(caf::sec::invalid_argument, std::move(msg)));
  }

  // -- DSL types for regular fields -------------------------------------------

  template <class T, class Predicate>
  struct field_with_invariant_and_fallback_t {
    string_view field_name;
    T* val;
    T fallback;
    Predicate predicate;

    template <class Inspector>
    bool operator()(string_view object_name, Inspector& f) {
      bool is_present = false;
      if (!f.begin_field(field_name, is_present))
        return stop;
      if (is_present) {
        if (inspector_access<T>::apply(f, *val) && f.end_field()) {
          if (predicate(*val))
            return ok;
          set_invariant_check_error(f, field_name, object_name);
        }
        return stop;
      }
      *val = std::move(fallback);
      return f.end_field();
    }
  };

  template <class T>
  struct field_with_fallback_t {
    string_view field_name;
    T* val;
    T fallback;

    template <class Inspector>
    bool operator()(string_view, Inspector& f) {
      bool is_present = false;
      if (!f.begin_field(field_name, is_present))
        return stop;
      if (is_present)
        return inspector_access<T>::apply(f, *val) && f.end_field();
      *val = std::move(fallback);
      return f.end_field();
    }

    template <class Predicate>
    auto invariant(Predicate predicate) && {
      return field_with_invariant_and_fallback_t<T, Predicate>{
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
    bool operator()(string_view object_name, Inspector& f) {
      if (f.begin_field(field_name)              //
          && inspector_access<T>::apply(f, *val) //
          && f.end_field()) {
        if (predicate(*val))
          return ok;
        set_invariant_check_error(f, field_name, object_name);
      }
      return stop;
    }

    auto fallback(T value) && {
      return field_with_invariant_and_fallback_t<T, Predicate>{
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
    bool operator()(string_view, Inspector& f) {
      if constexpr (inspector_access_traits<T>::is_optional) {
        bool is_present = false;
        if (!f.begin_field(field_name, is_present))
          return stop;
        using value_type = std::decay_t<decltype(**val)>;
        if (is_present) {
          auto tmp = value_type{};
          if (!inspector_access<value_type>::apply(f, tmp))
            return stop;
          *val = std::move(tmp);
          return f.end_field();
        } else {
          *val = T{};
          return f.end_field();
        }
      } else {
        return f.begin_field(field_name)              //
               && inspector_access<T>::apply(f, *val) //
               && f.end_field();
      }
    }

    auto fallback(T value) && {
      return field_with_fallback_t<T>{field_name, val, std::move(value)};
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

  template <class T, class Set, class Predicate>
  struct virt_field_with_invariant_and_fallback_t {
    string_view field_name;
    Set set;
    T fallback;
    Predicate predicate;

    template <class Inspector>
    bool operator()(string_view object_name, Inspector& f) {
      bool is_present = false;
      if (!f.begin_field(field_name, is_present))
        return stop;
      if (is_present) {
        auto tmp = T{};
        if (!inspector_access<T>::apply(f, tmp))
          return stop;
        if (!predicate(tmp)) {
          set_invariant_check_error(f, field_name, object_name);
          return stop;
        }
        if (!set(std::move(tmp))) {
          set_field_store_error(f, field_name, object_name);
          return stop;
        }
        return f.end_field();
      }
      if (!set(std::move(fallback))) {
        set_field_store_error(f, field_name, object_name);
        return stop;
      }
      return f.end_field();
    }
  };

  template <class T, class Set>
  struct virt_field_with_fallback_t {
    string_view field_name;
    Set set;
    T fallback;

    template <class Inspector>
    bool operator()(string_view object_name, Inspector& f) {
      bool is_present = false;
      if (!f.begin_field(field_name, is_present))
        return stop;
      if (is_present) {
        auto tmp = T{};
        if (!inspector_access<T>::apply(f, tmp))
          return stop;
        if (!set(std::move(tmp))) {
          set_field_store_error(f, field_name, object_name);
          return stop;
        }
        return f.end_field();
      }
      if (!set(std::move(fallback))) {
        set_field_store_error(f, field_name, object_name);
        return stop;
      }
      return f.end_field();
    }
  };

  template <class T, class Set, class Predicate>
  struct virt_field_with_invariant_t {
    string_view field_name;
    Set set;
    Predicate predicate;

    template <class Inspector>
    bool operator()(string_view object_name, Inspector& f) {
      if (!f.begin_field(field_name))
        return stop;
      auto tmp = T{};
      if (!inspector_access<T>::apply(f, tmp))
        return stop;
      if (!predicate(tmp)) {
        set_invariant_check_error(f, field_name, object_name);
        return stop;
      }
      if (!set(std::move(tmp))) {
        set_field_store_error(f, field_name, object_name);
        return stop;
      }
      return f.end_field();
    }

    auto fallback(T value) && {
      return virt_field_with_invariant_and_fallback_t<T, Set, Predicate>{
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
    bool operator()(string_view, Inspector& f) {
      if (!f.begin_field(field_name))
        return stop;
      auto tmp = T{};
      if (!inspector_access<T>::apply(f, tmp))
        return stop;
      if (!set(std::move(tmp))) {
      }
      return f.end_field();
    }

    auto fallback(T value) && {
      return virt_field_with_fallback_t<T, Set>{
        field_name,
        std::move(set),
        std::move(value),
      };
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
  static auto field(string_view name, Get get, Set set) {
    using field_type = std::decay_t<decltype(get())>;
    using set_result = decltype(set(std::declval<field_type>()));
    static_assert(std::is_same<set_result, bool>::value,
                  "setters of fields must return bool");
    return virt_field_t<field_type, Set>{name, set};
  }
};

} // namespace caf
