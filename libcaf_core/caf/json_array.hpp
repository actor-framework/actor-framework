// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/json.hpp"
#include "caf/fwd.hpp"
#include "caf/json_value.hpp"

#include <iterator>
#include <memory>

namespace caf {

/// Represents a JSON array.
class CAF_CORE_EXPORT json_array {
public:
  // -- friends ----------------------------------------------------------------

  friend class json_value;

  // -- member types -----------------------------------------------------------

  class const_iterator {
  public:
    using difference_type = ptrdiff_t;

    using value_type = json_value;

    using pointer = value_type*;

    using reference = value_type&;

    using iterator_category = std::forward_iterator_tag;

    const_iterator(detail::json::value::array::const_iterator iter,
                   detail::json::storage* storage)
      : iter_(iter), storage_(storage) {
      // nop
    }

    const_iterator() noexcept : storage_(nullptr) {
      // nop
    }

    const_iterator(const const_iterator&) = default;

    const_iterator& operator=(const const_iterator&) = default;

    json_value value() const noexcept {
      return json_value{std::addressof(*iter_), storage_};
    }

    json_value operator*() const noexcept {
      return value();
    }

    const_iterator& operator++() noexcept {
      ++iter_;
      return *this;
    }

    const_iterator operator++(int) noexcept {
      return {iter_++, storage_};
    }

    bool equal_to(const const_iterator& other) const noexcept {
      return iter_ == other.iter_;
    }

  private:
    detail::json::value::array::const_iterator iter_;
    detail::json::storage* storage_;
  };

  // -- constructors, destructors, and assignment operators --------------------

  json_array() noexcept;

  json_array(json_array&&) noexcept = default;

  json_array(const json_array&) noexcept = default;

  json_array& operator=(json_array&&) noexcept = default;

  json_array& operator=(const json_array&) noexcept = default;

  // -- properties -------------------------------------------------------------

  /// Checks whether the array has no members.
  bool empty() const noexcept {
    return arr_->empty();
  }

  /// Alias for @c empty.
  bool is_empty() const noexcept {
    return empty();
  }

  /// Returns the number of key-value pairs in this array.
  size_t size() const noexcept {
    return arr_->size();
  }

  const_iterator begin() const noexcept {
    return {arr_->begin(), storage_.get()};
  }

  const_iterator end() const noexcept {
    return {arr_->end(), storage_.get()};
  }

  // -- printing ---------------------------------------------------------------

  template <class Buffer>
  void print_to(Buffer& buf, size_t indentation_factor = 0) const {
    detail::json::print_to(buf, *arr_, indentation_factor);
  }

  // -- serialization ----------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& inspector, json_array& arr) {
    if constexpr (Inspector::is_loading) {
      auto storage = make_counted<detail::json::storage>();
      auto* internal_arr = detail::json::make_array(storage);
      if (!detail::json::load(inspector, *internal_arr, storage))
        return false;
      arr = json_array{internal_arr, std::move(storage)};
      return true;
    } else {
      return detail::json::save(inspector, *arr.arr_);
    }
  }

private:
  json_array(const detail::json::value::array* obj,
             detail::json::storage_ptr sptr) noexcept
    : arr_(obj), storage_(sptr) {
    // nop
  }

  const detail::json::value::array* arr_ = nullptr;
  detail::json::storage_ptr storage_;
};

// -- free functions -----------------------------------------------------------

inline bool operator==(const json_array::const_iterator& lhs,
                       const json_array::const_iterator& rhs) noexcept {
  return lhs.equal_to(rhs);
}

inline bool operator!=(const json_array::const_iterator& lhs,
                       const json_array::const_iterator& rhs) noexcept {
  return !lhs.equal_to(rhs);
}

inline bool operator==(const json_array& lhs, const json_array& rhs) noexcept {
  return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

inline bool operator!=(const json_array& lhs, const json_array& rhs) noexcept {
  return !(lhs == rhs);
}

/// @relates json_array
CAF_CORE_EXPORT std::string to_string(const json_array& arr);

} // namespace caf
