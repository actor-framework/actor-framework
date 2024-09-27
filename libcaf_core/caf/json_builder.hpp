// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/json.hpp"
#include "caf/json_value.hpp"
#include "caf/json_writer.hpp"
#include "caf/serializer.hpp"

namespace caf {

/// Serializes an inspectable object to a @ref json_value.
class CAF_CORE_EXPORT json_builder : public serializer {
public:
  // -- member types -----------------------------------------------------------

  using super = serializer;

  using type = json_writer::type;

  // -- constructors, destructors, and assignment operators --------------------

  json_builder() {
    init();
  }

  explicit json_builder(actor_system& sys) : super(sys) {
    init();
  }

  json_builder(const json_builder&) = delete;

  json_builder& operator=(const json_builder&) = delete;

  ~json_builder() override;

  // -- properties -------------------------------------------------------------

  /// Returns whether the writer omits empty fields entirely (true) or renders
  /// empty fields as `$field: null` (false).
  [[nodiscard]] bool skip_empty_fields() const noexcept {
    return skip_empty_fields_;
  }

  /// Configures whether the writer omits empty fields.
  void skip_empty_fields(bool value) noexcept {
    skip_empty_fields_ = value;
  }

  /// Returns whether the writer omits `@type` annotations for JSON objects.
  [[nodiscard]] bool skip_object_type_annotation() const noexcept {
    return skip_object_type_annotation_;
  }

  /// Configures whether the writer omits `@type` annotations for JSON objects.
  void skip_object_type_annotation(bool value) noexcept {
    skip_object_type_annotation_ = value;
  }

  /// Returns the suffix for generating type annotation fields for variant
  /// fields. For example, CAF inserts field called "@foo${field_type_suffix}"
  /// for a variant field called "foo".
  [[nodiscard]] std::string_view field_type_suffix() const noexcept {
    return field_type_suffix_;
  }

  /// Configures whether the writer omits empty fields.
  void field_type_suffix(std::string_view suffix) noexcept {
    field_type_suffix_ = suffix;
  }

  // -- modifiers --------------------------------------------------------------

  /// Restores the writer to its initial state.
  void reset();

  /// Seals the JSON value, i.e., rendering it immutable, and returns it. After
  /// calling this member function, the @ref json_builder is in a moved-from
  /// state and users may only call @c reset to start a new building process or
  /// destroy this instance.
  json_value seal();

  // -- overrides --------------------------------------------------------------

  bool begin_object(type_id_t type, std::string_view name) override;

  bool end_object() override;

  bool begin_field(std::string_view) override;

  bool begin_field(std::string_view name, bool is_present) override;

  bool begin_field(std::string_view name, span<const type_id_t> types,
                   size_t index) override;

  bool begin_field(std::string_view name, bool is_present,
                   span<const type_id_t> types, size_t index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_key_value_pair() override;

  bool end_key_value_pair() override;

  bool begin_sequence(size_t size) override;

  bool end_sequence() override;

  bool begin_associative_array(size_t size) override;

  bool end_associative_array() override;

  bool value(std::byte x) override;

  bool value(bool x) override;

  bool value(int8_t x) override;

  bool value(uint8_t x) override;

  bool value(int16_t x) override;

  bool value(uint16_t x) override;

  bool value(int32_t x) override;

  bool value(uint32_t x) override;

  bool value(int64_t x) override;

  bool value(uint64_t x) override;

  bool value(float x) override;

  bool value(double x) override;

  bool value(long double x) override;

  bool value(std::string_view x) override;

  bool value(const std::u16string& x) override;

  bool value(const std::u32string& x) override;

  bool value(span<const std::byte> x) override;

private:
  // -- implementation details -------------------------------------------------

  template <class T>
  bool number(T);

  using key_type = std::string_view;

  // -- state management -------------------------------------------------------

  void init();

  // Returns the current top of the stack or `null` if empty.
  type top();

  // Returns the current top of the stack or `null` if empty.
  template <class T = detail::json::value>
  T* top_ptr();

  // Returns the current top-level object.
  detail::json::object* top_obj();

  // Enters a new level of nesting.
  void push(detail::json::value*, type);

  // Enters a new level of nesting with type member.
  void push(detail::json::value::member*);

  // Enters a new level of nesting with type key.
  void push(key_type*);

  // Backs up one level of nesting.
  bool pop();

  // Backs up one level of nesting but checks that current top is `t` before.
  bool pop_if(type t);

  // Sets an error reason that the inspector failed to write a t.
  void fail(type t);

  // Checks whether any element in the stack has the type `object`.
  bool inside_object() const noexcept;

  // -- member variables -------------------------------------------------------

  // Our output.
  detail::json::value* val_;

  // Storage for the assembled output.
  detail::json::storage_ptr storage_;

  struct entry {
    union {
      detail::json::value* val_ptr;
      detail::json::member* mem_ptr;
      key_type* key_ptr;
    };
    type t;

    entry(detail::json::value* ptr, type ptr_type) noexcept {
      val_ptr = ptr;
      t = ptr_type;
    }

    explicit entry(detail::json::member* ptr) noexcept {
      mem_ptr = ptr;
      t = type::member;
    }

    explicit entry(key_type* ptr) noexcept {
      key_ptr = ptr;
      t = type::key;
    }

    entry(const entry&) noexcept = default;

    entry& operator=(const entry&) noexcept = default;
  };

  // Bookkeeping for where we are in the current object.
  std::vector<entry> stack_;

  // Configures whether we omit empty fields entirely (true) or render empty
  // fields as `$field: null` (false).
  bool skip_empty_fields_ = json_writer::skip_empty_fields_default;

  // Configures whether we omit the top-level `@type` annotation.
  bool skip_object_type_annotation_ = false;

  std::string_view field_type_suffix_ = json_writer::field_type_suffix_default;
};

} // namespace caf
