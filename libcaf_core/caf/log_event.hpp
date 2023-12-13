// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/chunked_string.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/format.hpp"
#include "caf/detail/json.hpp"
#include "caf/detail/monotonic_buffer_resource.hpp"
#include "caf/detail/source_location.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace caf {

// -- log_event ----------------------------------------------------------------

/// Captures a single event for a logger.
class CAF_CORE_EXPORT log_event : public ref_counted {
public:
  // -- member types -----------------------------------------------------------

  friend class log_event_builder;

  struct field;

  using field_node = detail::json::linked_list_node<field>;

  /// A list of user-defined fields.
  struct field_list {
    const field_node* head;
    auto begin() const noexcept {
      return detail::json::linked_list_iterator<const field>{head};
    }
    auto end() const noexcept {
      return detail::json::linked_list_iterator<const field>{};
    }
  };

  /// A single, user-defined field.
  struct field {
    using value_t
      = std::variant<std::nullopt_t, bool, int64_t, uint64_t, double,
                     std::string_view, chunked_string, field_list>;

    /// The key (name) of the field.
    std::string_view key;

    /// The value of the field.
    value_t value;
  };

  // -- constructors, destructors, and assignment operators --------------------

  log_event(unsigned level, std::string_view component,
            const detail::source_location& loc) noexcept
    : level_(level),
      component_(component),
      line_number_(loc.line()),
      file_name_(loc.file_name()),
      function_name_(loc.function_name()) {
    // nop
  }

  log_event(const log_event&) = delete;

  log_event& operator=(const log_event&) = delete;

  // -- factory functions ------------------------------------------------------

  template <class Arg, class... Args>
  static log_event_ptr make(unsigned level, std::string_view component,
                            const detail::source_location& loc,
                            std::string_view fmt, Arg&& arg, Args&&... args) {
    auto event = make(level, component, loc);
    chunked_string_builder cs_builder{&event->resource_};
    chunked_string_builder_output_iterator out{&cs_builder};
    detail::format_to(out, fmt, std::forward<Arg>(arg),
                      std::forward<Args>(args)...);
    event->message_ = cs_builder.build();
    return event;
  }

  static log_event_ptr make(unsigned level, std::string_view component,
                            const detail::source_location& loc,
                            std::string_view msg);

  // -- properties -------------------------------------------------------------

  /// Returns the severity level of the event.
  [[nodiscard]] unsigned level() const noexcept {
    return level_;
  }

  /// Returns the name of the component that generated the event.
  [[nodiscard]] std::string_view component() const noexcept {
    return component_;
  }

  /// Returns the line number at which the event was generated.
  [[nodiscard]] uint_least32_t line_number() const noexcept {
    return line_number_;
  }

  /// Returns the name of the file in which the event was generated.
  [[nodiscard]] const char* file_name() const noexcept {
    return file_name_;
  }

  /// Returns the name of the function in which the event was generated.
  [[nodiscard]] const char* function_name() const noexcept {
    return function_name_;
  }

  /// Returns the user-defined message of the event.
  [[nodiscard]] chunked_string message() const noexcept {
    return message_;
  }

  /// Returns the user-defined fields of the event.
  [[nodiscard]] field_list fields() const noexcept {
    return field_list{first_field_};
  }

private:
  static log_event_ptr make(unsigned level, std::string_view component,
                            const detail::source_location& loc);

  /// The severity level of the event.
  unsigned level_;

  /// The name of the component that generated the event.
  std::string_view component_;

  /// The line number at which the event was generated.
  uint_least32_t line_number_;

  /// The name of the file in which the event was generated.
  const char* file_name_;

  /// The name of the function in which the event was generated.
  const char* function_name_;

  /// The user-defined message of the event.
  chunked_string message_;

  /// Pointer to the first user-defined field of the event.
  const field_node* first_field_ = nullptr;

  /// Storage for string chunks and fields.
  detail::monotonic_buffer_resource resource_;
};

/// Builds list of user-defined fields for a log event.
class CAF_CORE_EXPORT log_event_fields_builder {
private:
  template <class T>
  static auto lift_integral(T value) {
    if constexpr (std::is_same_v<T, bool>)
      return value;
    else if constexpr (std::is_signed_v<T>)
      return static_cast<int64_t>(value);
    else
      return static_cast<uint64_t>(value);
  }

public:
  // -- member types -----------------------------------------------------------

  using list_type = detail::json::linked_list<log_event::field>;

  using resource_type = detail::monotonic_buffer_resource;

  // -- constructors, destructors, and assignment operators --------------------

  explicit log_event_fields_builder(resource_type* resource) noexcept;

  ~log_event_fields_builder() noexcept {
    // nop
  }

  // -- add fields -------------------------------------------------------------

  /// Adds a boolean or integer field.
  template <class T>
  std::enable_if_t<std::is_integral_v<T>>
  add_field(std::string_view key, T value) {
    auto& field = fields_.emplace_back(std::string_view{},
                                       lift_integral(value));
    field.key = deep_copy(key);
  }

  /// Adds a floating point field.
  void add_field(std::string_view key, double value) {
    auto& field = fields_.emplace_back(std::string_view{}, value);
    field.key = deep_copy(key);
  }

  /// Adds a string field.
  void add_field(std::string_view key, std::string_view value) {
    auto& field = fields_.emplace_back(std::string_view{}, std::nullopt);
    field.key = deep_copy(key);
    field.value = deep_copy(value);
  }

  /// Adds a formatted string field.
  template <class Arg, class... Args>
  void add_field(std::string_view key, std::string_view fmt, Arg&& arg,
                 Args&&... args) {
    auto& field = fields_.emplace_back(std::string_view{}, std::nullopt);
    field.key = deep_copy(key);
    chunked_string_builder cs_builder{resource()};
    chunked_string_builder_output_iterator out{&cs_builder};
    detail::format_to(out, fmt, std::forward<Arg>(arg),
                      std::forward<Args>(args)...);
    field.value = cs_builder.build();
  }

  /// Adds nested fields.
  template <class SubFieldsInitializer>
  auto add_field(std::string_view key, SubFieldsInitializer&& init)
    -> std::enable_if_t<std::is_same_v<
      decltype(init(std::declval<log_event_fields_builder&>())), void>> {
    auto& field = fields_.emplace_back(std::string_view{}, std::nullopt);
    field.key = deep_copy(key);
    log_event_fields_builder nested_builder{resource()};
    init(nested_builder);
    field.value = nested_builder.build();
  }

  // -- build ------------------------------------------------------------------

  /// Seals the list and returns it.
  [[nodiscard]] log_event::field_list build() {
    return log_event::field_list{fields_.head()};
  }

private:
  [[nodiscard]] resource_type* resource() noexcept {
    return fields_.get_allocator().resource();
  }

  [[nodiscard]] std::string_view deep_copy(std::string_view str);

  union { // Suppress the dtor call for fields_.
    list_type fields_;
  };
};

/// Builds a chunked string by allocating each chunk on a monotonic buffer.
class CAF_CORE_EXPORT log_event_builder {
public:
  // -- member types -----------------------------------------------------------

  using resource_type = detail::monotonic_buffer_resource;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Args>
  log_event_builder(unsigned level, std::string_view component,
                    const detail::source_location& loc, std::string_view fmt,
                    Args&&... args)
    : event_(
      log_event::make(level, component, loc, fmt, std::forward<Args>(args)...)),
      fields_(&event_->resource_) {
    // nop
  }

  // -- add fields -------------------------------------------------------------

  /// Adds a boolean or integer field.
  template <class T>
  std::enable_if_t<std::is_integral_v<T>, log_event_builder&&>
  add_field(std::string_view key, T value) && {
    fields_.add_field(key, value);
    return std::move(*this);
  }

  /// Adds a floating point field.
  log_event_builder&& add_field(std::string_view key, double value) && {
    fields_.add_field(key, value);
    return std::move(*this);
  }

  /// Adds a string field.
  log_event_builder&& add_field(std::string_view key,
                                std::string_view value) && {
    fields_.add_field(key, value);
    return std::move(*this);
  }

  /// Adds a formatted string field.
  template <class Arg, class... Args>
  log_event_builder&& add_field(std::string_view key, std::string_view fmt,
                                Arg&& arg, Args&&... args) && {
    fields_.add_field(key, fmt, std::forward<Arg>(arg),
                      std::forward<Args>(args)...);
    return std::move(*this);
  }

  /// Adds nested fields.
  template <class SubFieldsInitializer>
  auto add_field(std::string_view key, SubFieldsInitializer&& init)
    -> std::enable_if_t<
      std::is_same_v<decltype(init(std::declval<log_event_fields_builder&>())),
                     void>,
      log_event_builder&&> {
    fields_.add_field(key, std::forward<SubFieldsInitializer>(init));
    return std::move(*this);
  }

  // -- build ------------------------------------------------------------------

  /// Seals the event and returns it.
  [[nodiscard]] log_event_ptr build() &&;

private:
  [[nodiscard]] resource_type* resource() noexcept {
    return &event_->resource_;
  }

  log_event_ptr event_;

  log_event_fields_builder fields_;
};

} // namespace caf
