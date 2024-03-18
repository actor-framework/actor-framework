// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/deserializer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/json.hpp"

#include <string_view>
#include <variant>

namespace caf {

/// Deserializes an inspectable object from a JSON-formatted string.
class CAF_CORE_EXPORT json_reader : public deserializer {
public:
  // -- member types -----------------------------------------------------------

  using super = deserializer;

  struct sequence {
    detail::json::array::const_iterator pos;

    detail::json::array::const_iterator end;

    bool at_end() const noexcept {
      return pos == end;
    }

    auto& current() {
      return *pos;
    }

    void advance() {
      ++pos;
    }
  };

  struct members {
    detail::json::object::const_iterator pos;

    detail::json::object::const_iterator end;

    bool at_end() const noexcept {
      return pos == end;
    }

    auto& current() {
      return *pos;
    }

    void advance() {
      ++pos;
    }
  };

  using json_key = std::string_view;

  using value_type
    = std::variant<const detail::json::value*, const detail::json::object*,
                   detail::json::null_t, json_key, sequence, members>;

  using stack_allocator
    = detail::monotonic_buffer_resource::allocator<value_type>;

  using stack_type = std::vector<value_type, stack_allocator>;

  /// Denotes the type at the current position.
  enum class position {
    value,
    object,
    null,
    key,
    sequence,
    members,
    past_the_end,
    invalid,
  };

  // -- constants --------------------------------------------------------------

  /// The value value for `field_type_suffix()`.
  static constexpr std::string_view field_type_suffix_default = "-type";

  // -- constructors, destructors, and assignment operators --------------------

  json_reader();

  explicit json_reader(actor_system& sys);

  json_reader(const json_reader&) = delete;

  json_reader& operator=(const json_reader&) = delete;

  ~json_reader() override;

  // -- properties -------------------------------------------------------------

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

  /// Parses @p json_text into an internal representation. After loading the
  /// JSON input, the reader is ready for attempting to deserialize inspectable
  /// objects.
  /// @warning The internal data structure keeps pointers into @p json_text.
  ///          Hence, the buffer pointed to by the string view must remain valid
  ///          until either destroying this reader or calling `reset`.
  /// @note Implicitly calls `reset`.
  bool load(std::string_view json_text);

  /// Parses the content of the file under the given @p path. After loading the
  /// content of the JSON file, the reader is ready for attempting to
  /// deserialize inspectable objects.
  /// @note Implicitly calls `reset`.
  bool load_file(const char* path);

  /// @copydoc load_file
  bool load_file(const std::string& path) {
    return load_file(path.c_str());
  }

  /// Reverts the state of the reader back to where it was after calling `load`.
  /// @post The reader is ready for attempting to deserialize another
  ///       inspectable object.
  void revert();

  /// Removes any loaded JSON data and reclaims memory resources.
  void reset();

  // -- overrides --------------------------------------------------------------

  bool fetch_next_object_type(type_id_t& type) override;

  bool fetch_next_object_name(std::string_view& type_name) override;

  bool begin_object(type_id_t type, std::string_view name) override;

  bool end_object() override;

  bool begin_field(std::string_view) override;

  bool begin_field(std::string_view name, bool& is_present) override;

  bool begin_field(std::string_view name, span<const type_id_t> types,
                   size_t& index) override;

  bool begin_field(std::string_view name, bool& is_present,
                   span<const type_id_t> types, size_t& index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_key_value_pair() override;

  bool end_key_value_pair() override;

  bool begin_sequence(size_t& size) override;

  bool end_sequence() override;

  bool begin_associative_array(size_t& size) override;

  bool end_associative_array() override;

  bool value(std::byte& x) override;

  bool value(bool& x) override;

  bool value(int8_t& x) override;

  bool value(uint8_t& x) override;

  bool value(int16_t& x) override;

  bool value(uint16_t& x) override;

  bool value(int32_t& x) override;

  bool value(uint32_t& x) override;

  bool value(int64_t& x) override;

  bool value(uint64_t& x) override;

  bool value(float& x) override;

  bool value(double& x) override;

  bool value(long double& x) override;

  bool value(std::string& x) override;

  bool value(std::u16string& x) override;

  bool value(std::u32string& x) override;

  bool value(span<std::byte> x) override;

private:
  [[nodiscard]] position pos() const noexcept;

  void append_current_field_name(std::string& str);

  std::string current_field_name();

  template <bool PopOrAdvanceOnSuccess, class F>
  bool consume(const char* fun_name, F f);

  template <class T>
  bool integer(T& x);

  template <position P>
  auto& top() noexcept {
    return std::get<static_cast<size_t>(P)>(st_->back());
  }

  template <position P>
  const auto& top() const noexcept {
    return std::get<static_cast<size_t>(P)>(st_->back());
  }

  void pop() {
    st_->pop_back();
  }

  template <class T>
  void push(T&& x) {
    st_->emplace_back(std::forward<T>(x));
  }

  detail::monotonic_buffer_resource buf_;

  stack_type* st_ = nullptr;

  detail::json::value* root_ = nullptr;

  std::string_view field_type_suffix_ = field_type_suffix_default;

  /// Keeps track of the current field for better debugging output.
  std::vector<std::string_view> field_;

  /// The mapper implementation we use by default.
  default_type_id_mapper default_mapper_;

  /// Configures which ID mapper we use to translate between type IDs and names.
  const type_id_mapper* mapper_ = &default_mapper_;
};

} // namespace caf
