// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/as_mutable_ref.hpp"
#include "caf/sec.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Provides default implementations for `save_field` and `load_field`.
template <class T>
struct inspector_access_base {
  /// Loads a mandatory field from `f`.
  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, string_view field_name, T& x,
                         IsValid& is_valid, SyncValue& sync_value) {
    if (f.begin_field(field_name) && f.apply(x)) {
      if (!is_valid(x)) {
        f.emplace_error(sec::field_invariant_check_failed,
                        to_string(field_name));
        return false;
      }
      if (!sync_value()) {
        if (!f.get_error())
          f.emplace_error(sec::field_value_synchronization_failed,
                          to_string(field_name));
        return false;
      }
      return f.end_field();
    }
    return false;
  }

  /// Loads an optional field from `f`, calling `set_fallback` if the source
  /// contains no value for `x`.
  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, string_view field_name, T& x,
                         IsValid& is_valid, SyncValue& sync_value,
                         SetFallback& set_fallback) {
    bool is_present = false;
    if (!f.begin_field(field_name, is_present))
      return false;
    if (is_present) {
      if (!f.apply(x))
        return false;
      if (!is_valid(x)) {
        f.emplace_error(sec::field_invariant_check_failed,
                        to_string(field_name));
        return false;
      }
      if (!sync_value()) {
        if (!f.get_error())
          f.emplace_error(sec::field_value_synchronization_failed,
                          to_string(field_name));
        return false;
      }
      return f.end_field();
    }
    set_fallback();
    return f.end_field();
  }

  /// Saves a mandatory field to `f`.
  template <class Inspector>
  static bool save_field(Inspector& f, string_view field_name, T& x) {
    return f.begin_field(field_name) //
           && f.apply(x)             //
           && f.end_field();
  }

  /// Saves an optional field to `f`.
  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, string_view field_name,
                         IsPresent& is_present, Get& get) {
    if (is_present()) {
      auto&& x = get();
      return f.begin_field(field_name, true) //
             && f.apply(x)                   //
             && f.end_field();
    }
    return f.begin_field(field_name, false) && f.end_field();
  }
};

} // namespace caf
