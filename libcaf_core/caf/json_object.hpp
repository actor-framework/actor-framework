// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/json.hpp"
#include "caf/fwd.hpp"
#include "caf/json_value.hpp"

#include <iterator>

namespace caf {

/// Represents a JSON object.
class CAF_CORE_EXPORT json_object {
public:
  // -- friends ----------------------------------------------------------------

  friend class json_value;

  // -- member types -----------------------------------------------------------

  class const_iterator {
  public:
    using difference_type = ptrdiff_t;

    using value_type = std::pair<std::string_view, json_value>;

    using pointer = value_type*;

    using reference = value_type&;

    using iterator_category = std::forward_iterator_tag;

    const_iterator(detail::json::value::object::const_iterator iter,
                   detail::json::storage* storage)
      : iter_(iter), storage_(storage) {
      // nop
    }

    const_iterator() noexcept = default;

    const_iterator(const const_iterator&) = default;

    const_iterator& operator=(const const_iterator&) = default;

    std::string_view key() const noexcept {
      return iter_->key;
    }

    json_value value() const noexcept {
      return json_value{iter_->val, storage_};
    }

    value_type operator*() const noexcept {
      return {key(), value()};
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
    detail::json::value::object::const_iterator iter_;
    detail::json::storage* storage_ = nullptr;
  };

  // -- constructors, destructors, and assignment operators --------------------

  json_object() noexcept : obj_(detail::json::empty_object()) {
    // nop
  }

  json_object(json_object&&) noexcept = default;

  json_object(const json_object&) noexcept = default;

  json_object& operator=(json_object&&) noexcept = default;

  json_object& operator=(const json_object&) noexcept = default;

  // -- properties -------------------------------------------------------------

  /// Checks whether the object has no members.
  bool empty() const noexcept {
    return !obj_ || obj_->empty();
  }

  /// Alias for @c empty.
  bool is_empty() const noexcept {
    return empty();
  }

  /// Returns the number of key-value pairs in this object.
  size_t size() const noexcept {
    return obj_ ? obj_->size() : 0u;
  }

  /// Returns the value for @p key or an @c undefined value if the object does
  /// not contain a value for @p key.
  json_value value(std::string_view key) const;

  const_iterator begin() const noexcept {
    return {obj_->begin(), storage_.get()};
  }

  const_iterator end() const noexcept {
    return {obj_->end(), storage_.get()};
  }

  // -- printing ---------------------------------------------------------------

  template <class Buffer>
  void print_to(Buffer& buf, size_t indentation_factor = 0) const {
    detail::json::print_to(buf, *obj_, indentation_factor);
  }

  // -- serialization ----------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& inspector, json_object& obj) {
    if constexpr (Inspector::is_loading) {
      auto storage = make_counted<detail::json::storage>();
      auto* internal_obj = detail::json::make_object(storage);
      if (!detail::json::load(inspector, *internal_obj, storage))
        return false;
      obj = json_object{internal_obj, std::move(storage)};
      return true;
    } else {
      return detail::json::save(inspector, *obj.obj_);
    }
  }

private:
  json_object(const detail::json::value::object* obj,
              detail::json::storage_ptr sptr) noexcept
    : obj_(obj), storage_(sptr) {
    // nop
  }

  const detail::json::value::object* obj_ = nullptr;
  detail::json::storage_ptr storage_;
};

inline bool operator==(const json_object::const_iterator& lhs,
                       const json_object::const_iterator& rhs) noexcept {
  return lhs.equal_to(rhs);
}

inline bool operator!=(const json_object::const_iterator& lhs,
                       const json_object::const_iterator& rhs) noexcept {
  return !lhs.equal_to(rhs);
}
inline bool operator==(const json_object& lhs,
                       const json_object& rhs) noexcept {
  return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

inline bool operator!=(const json_object& lhs,
                       const json_object& rhs) noexcept {
  return !(lhs == rhs);
}

/// @relates json_object
CAF_CORE_EXPORT std::string to_string(const json_object& val);

} // namespace caf
