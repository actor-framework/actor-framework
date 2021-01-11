// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <vector>

#include "caf/detail/core_export.hpp"
#include "caf/serializer.hpp"

namespace caf {

/// Serializes an inspectable object to a JSON-formatted string.
class CAF_CORE_EXPORT json_writer : public serializer {
public:
  // -- member types -----------------------------------------------------------

  using super = serializer;

  /// Reflects the structure of JSON objects according to ECMA-404. This enum
  /// skips types such as `members` or `value` since they are not needed to
  /// generate JSON.
  enum class type : uint8_t {
    element, /// Can morph into any other type except `member`.
    object,  /// Contains any number of members.
    member,  /// A single object member (key/value pair).
    array,   /// Contains any number of elements.
    string,  /// A character sequence (terminal type).
    number,  /// An integer or floating point (terminal type).
    boolean, /// Either "true" or "false" (terminal type).
    null,    /// The literal "null" (terminal type).
  };

  // -- constructors, destructors, and assignment operators --------------------

  json_writer();

  explicit json_writer(actor_system& sys);

  explicit json_writer(execution_unit* ctx);

  ~json_writer() override;

  // -- properties -------------------------------------------------------------

  /// Returns a string view into the internal buffer.
  /// @warning This view becomes invalid when calling any non-const member
  ///          function on the writer object.
  [[nodiscard]] string_view str() const noexcept {
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

  // -- modifiers --------------------------------------------------------------

  /// Removes all characters from the buffer and restores the writer to its
  /// initial state.
  /// @warning Invalidates all string views into the buffer.
  void reset();

  // -- overrides --------------------------------------------------------------

  bool begin_object(type_id_t type, string_view name) override;

  bool end_object() override;

  bool begin_field(string_view) override;

  bool begin_field(string_view name, bool is_present) override;

  bool begin_field(string_view name, span<const type_id_t> types,
                   size_t index) override;

  bool begin_field(string_view name, bool is_present,
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

  bool value(byte x) override;

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

  bool value(string_view x) override;

  bool value(const std::u16string& x) override;

  bool value(const std::u32string& x) override;

  bool value(span<const byte> x) override;

private:
  // -- implementation details -------------------------------------------------

  template <class T>
  bool number(T);

  // -- state management -------------------------------------------------------

  void init();

  // Returns the current top of the stack or `null_literal` if empty.
  type top();

  // Enters a new level of nesting.
  void push(type = type::element);

  // Enters a new level of nesting if the current top is `expected`.
  bool push_if(type expected, type = type::element);

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

  // -- printing ---------------------------------------------------------------

  // Adds a newline unless `compact() == true`.
  void nl();

  // Adds `c` to the output buffer.
  void add(char c) {
    buf_.push_back(c);
  }

  // Adds `str` to the output buffer.
  void add(string_view str) {
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
};

} // namespace caf
