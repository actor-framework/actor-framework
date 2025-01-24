// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/byte_writer.hpp"
#include "caf/detail/core_export.hpp"

#include <vector>

namespace caf {

/// Serializes an inspectable object to a JSON-formatted string.
class CAF_CORE_EXPORT json_writer : public byte_writer {
public:
  // -- member types -----------------------------------------------------------

  using super = byte_writer;

  /// Reflects the structure of JSON objects according to ECMA-404. This enum
  /// skips types such as `members` or `value` since they are not needed to
  /// generate JSON.
  enum class type : uint8_t {
    element, /// Can morph into any other type except `member`.
    object,  /// Contains any number of members.
    member,  /// A single key-value pair.
    key,     /// The key of a field.
    array,   /// Contains any number of elements.
    string,  /// A character sequence (terminal type).
    number,  /// An integer or floating point (terminal type).
    boolean, /// Either "true" or "false" (terminal type).
    null,    /// The literal "null" (terminal type).
  };

  // -- constants --------------------------------------------------------------

  /// The default value for `skip_empty_fields()`.
  static constexpr bool skip_empty_fields_default = true;

  /// The default value for `skip_object_type_annotation()`.
  static constexpr bool skip_object_type_annotation_default = false;

  /// The value value for `field_type_suffix()`.
  static constexpr std::string_view field_type_suffix_default = "-type";

  // -- constructors, destructors, and assignment operators --------------------

  json_writer() {
    init();
  }

  explicit json_writer(actor_system& sys) : super(sys) {
    init();
  }

  ~json_writer() override;

  // -- properties -------------------------------------------------------------

  span<const std::byte> bytes() const override;

  /// Returns a string view into the internal buffer.
  /// @warning This view becomes invalid when calling any non-const member
  ///          function on the writer object.
  [[nodiscard]] std::string_view str() const noexcept {
    return {buf_.data(), buf_.size()};
  }

  /// Returns the current indentation factor.
  [[nodiscard]] size_t indentation() const noexcept {
    return indentation_factor_;
  }

  /// Sets the indentation level.
  /// @param factor The number of spaces to add to each level of indentation. A
  ///               value of 0 (the default) disables indentation, printing the
  ///               entire JSON output into a single line.
  void indentation(size_t factor) noexcept {
    indentation_factor_ = factor;
  }

  /// Returns whether the writer generates compact JSON output without any
  /// spaces or newlines to separate values.
  [[nodiscard]] bool compact() const noexcept {
    return indentation_factor_ == 0;
  }

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

  /// Returns the type ID mapper used by the writer.
  [[nodiscard]] const type_id_mapper* mapper() const noexcept {
    return mapper_;
  }

  /// Changes the type ID mapper for the writer.
  void mapper(const type_id_mapper* ptr) noexcept {
    mapper_ = ptr;
  }

  // -- modifiers --------------------------------------------------------------

  /// Removes all characters from the buffer and restores the writer to its
  /// initial state.
  /// @warning Invalidates all string views into the buffer.
  void reset() override;

  // -- overrides --------------------------------------------------------------

  bool has_human_readable_format() const noexcept override;

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

  // -- state management -------------------------------------------------------

  void init();

  // Returns the current top of the stack or `null` if empty.
  type top();

  // Enters a new level of nesting.
  void push(type = type::element);

  // Backs up one level of nesting.
  bool pop();

  // Backs up one level of nesting but checks that current top is `t` before.
  bool pop_if(type t);

  // Backs up one level of nesting but checks that the top is `t` afterwards.
  bool pop_if_next(type t);

  // Tries to morph the current top of the stack to t.
  bool morph(type t);

  // Tries to morph the current top of the stack to t. Stores the previous value
  // to `prev`.
  bool morph(type t, type& prev);

  // Morphs the current top of the stack to t without performing *any* checks.
  void unsafe_morph(type t);

  // Sets an error reason that the inspector failed to write a t.
  void fail(type t);

  // Checks whether any element in the stack has the type `object`.
  bool inside_object() const noexcept;

  // -- printing ---------------------------------------------------------------

  // Adds a newline unless `compact() == true`.
  void nl();

  // Adds `c` to the output buffer.
  void add(char c) {
    buf_.push_back(c);
  }

  // Adds `str` to the output buffer.
  void add(std::string_view str) {
    buf_.insert(buf_.end(), str.begin(), str.end());
  }

  // Adds a separator to the output buffer unless the current entry is empty.
  // The separator is just a comma when in compact mode and otherwise a comma
  // followed by a newline.
  void sep();

  // -- member variables -------------------------------------------------------

  // The current level of indentation.
  size_t indentation_level_ = 0;

  // The number of whitespaces to add per indentation level.
  size_t indentation_factor_ = 0;

  // Buffer for producing the JSON output.
  std::vector<char> buf_;

  struct entry {
    type t;
    bool filled;
    friend bool operator==(entry x, type y) noexcept {
      return x.t == y;
    };
  };

  // Bookkeeping for where we are in the current object.
  std::vector<entry> stack_;

  // Configures whether we omit empty fields entirely (true) or render empty
  // fields as `$field: null` (false).
  bool skip_empty_fields_ = skip_empty_fields_default;

  // Configures whether we omit the top-level `@type` annotation.
  bool skip_object_type_annotation_ = false;

  // Configures how we generate type annotations for fields.
  std::string_view field_type_suffix_ = field_type_suffix_default;

  // The mapper implementation we use by default.
  default_type_id_mapper default_mapper_;

  // Configures which ID mapper we use to translate between type IDs and names.
  const type_id_mapper* mapper_ = &default_mapper_;
};

/// @relates json_writer::type
CAF_CORE_EXPORT std::string_view as_json_type_name(json_writer::type t);

/// @relates json_writer::type
constexpr bool can_morph(json_writer::type from, json_writer::type to) {
  return from == json_writer::type::element && to != json_writer::type::member;
}

} // namespace caf
