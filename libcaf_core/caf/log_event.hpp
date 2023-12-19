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
#include <thread>
#include <variant>

namespace caf {

/// Tag type for `log_event::with_message` that indicates that the event should
/// keep its original timestamp.
struct keep_timestamp_t {};

/// Configures `log_event::with_message` to keep the original timestamp.
constexpr keep_timestamp_t keep_timestamp{};

/// Captures a single event for a logger.
class CAF_CORE_EXPORT log_event : public ref_counted {
public:
  // -- member types -----------------------------------------------------------

  friend class log_event_sender;

  struct field;

  using field_node = detail::json::linked_list_node<field>;

  /// A list of user-defined fields.
  struct field_list {
    const field_node* head = nullptr;
    auto begin() const noexcept {
      return detail::json::linked_list_iterator<const field>{head};
    }
    auto end() const noexcept {
      return detail::json::linked_list_iterator<const field>{};
    }
    [[nodiscard]] bool empty() const noexcept {
      return head == nullptr;
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
            const detail::source_location& loc, caf::actor_id aid) noexcept
    : level_(level),
      component_(component),
      line_number_(loc.line()),
      file_name_(loc.file_name()),
      function_name_(loc.function_name()),
      aid_(aid),
      timestamp_(caf::make_timestamp()),
      tid_(std::this_thread::get_id()) {
    // nop
  }

  log_event(const log_event&) = delete;

  log_event& operator=(const log_event&) = delete;

  // -- factory functions ------------------------------------------------------

  template <class Arg, class... Args>
  static log_event_ptr make(unsigned level, std::string_view component,
                            const detail::source_location& loc,
                            caf::actor_id aid, std::string_view fmt, Arg&& arg,
                            Args&&... args) {
    auto event = make(level, component, loc, aid);
    chunked_string_builder cs_builder{&event->resource_};
    chunked_string_builder_output_iterator out{&cs_builder};
    detail::format_to(out, fmt, std::forward<Arg>(arg),
                      std::forward<Args>(args)...);
    event->message_ = cs_builder.build();
    return event;
  }

  static log_event_ptr make(unsigned level, std::string_view component,
                            const detail::source_location& loc,
                            caf::actor_id aid, std::string_view msg);

  /// Returns a deep copy of `this` with a new message without changing the
  /// timestamp.
  [[nodiscard]] log_event_ptr with_message(std::string_view msg,
                                           keep_timestamp_t) const;

  /// Returns a copy of `this` with a new message and an updated timestamp.
  [[nodiscard]] log_event_ptr with_message(std::string_view msg) const;

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

  /// Returns the ID of the actor that generated the event.
  [[nodiscard]] caf::actor_id actor_id() const noexcept {
    return aid_;
  }

  /// Returns the timestamp of the event.
  [[nodiscard]] caf::timestamp timestamp() const noexcept {
    return timestamp_;
  }

  /// Returns the ID of the thread that generated the event.
  [[nodiscard]] std::thread::id thread_id() const noexcept {
    return tid_;
  }

private:
  log_event() = default;

  static log_event_ptr make(unsigned level, std::string_view component,
                            const detail::source_location& loc,
                            caf::actor_id aid);

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

  /// The ID of the actor that generated the event.
  caf::actor_id aid_;

  /// The timestamp of the event.
  caf::timestamp timestamp_;

  /// The ID of the thread that generated the event.
  std::thread::id tid_;

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

  friend class log_event;

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
  std::enable_if_t<std::is_integral_v<T>, log_event_fields_builder&>
  field(std::string_view key, T value) {
    auto& field = fields_.emplace_back(std::string_view{},
                                       lift_integral(value));
    field.key = deep_copy(key);
    return *this;
  }

  /// Adds a floating point field.
  log_event_fields_builder& field(std::string_view key, double value) {
    auto& field = fields_.emplace_back(std::string_view{}, value);
    field.key = deep_copy(key);
    return *this;
  }

  /// Adds a string field.
  log_event_fields_builder& field(std::string_view key,
                                  std::string_view value) {
    auto& field = fields_.emplace_back(std::string_view{}, std::nullopt);
    field.key = deep_copy(key);
    field.value = deep_copy(value);
    return *this;
  }

  /// Adds a formatted string field.
  template <class Arg, class... Args>
  log_event_fields_builder&
  field(std::string_view key, std::string_view fmt, Arg&& arg, Args&&... args) {
    auto& field = fields_.emplace_back(std::string_view{}, std::nullopt);
    field.key = deep_copy(key);
    chunked_string_builder cs_builder{resource()};
    chunked_string_builder_output_iterator out{&cs_builder};
    detail::format_to(out, fmt, std::forward<Arg>(arg),
                      std::forward<Args>(args)...);
    field.value = cs_builder.build();
    return *this;
  }

  /// Adds nested fields.
  template <class SubFieldsInitializer>
  auto field(std::string_view key, SubFieldsInitializer&& init)
    -> std::enable_if_t<
      std::is_same_v<decltype(init(std::declval<log_event_fields_builder&>())),
                     void>,
      log_event_fields_builder&> {
    auto& field = fields_.emplace_back(std::string_view{}, std::nullopt);
    field.key = deep_copy(key);
    log_event_fields_builder nested_builder{resource()};
    init(nested_builder);
    field.value = nested_builder.build();
    return *this;
  }

  // -- build ------------------------------------------------------------------

  /// Seals the list and returns it.
  [[nodiscard]] log_event::field_list build() {
    return log_event::field_list{fields_.head()};
  }

private:
  void field(std::string_view key, std::nullopt_t) {
    auto& field = fields_.emplace_back(std::string_view{}, std::nullopt);
    field.key = deep_copy(key);
  }

  void field(std::string_view key, chunked_string);

  void field(std::string_view key, log_event::field_list);

  [[nodiscard]] resource_type* resource() noexcept {
    return fields_.get_allocator().resource();
  }

  [[nodiscard]] std::string_view deep_copy(std::string_view str);

  union { // Suppress the dtor call for fields_.
    list_type fields_;
  };
};

/// Builds a log event by allocating each field on a monotonic buffer and then
/// sends it to the current logger.
class CAF_CORE_EXPORT log_event_sender {
public:
  // -- member types -----------------------------------------------------------

  using resource_type = detail::monotonic_buffer_resource;

  // -- constructors, destructors, and assignment operators --------------------

  log_event_sender() : fields_(nullptr) {
    // nop
  }

  template <class... Args>
  log_event_sender(logger* ptr, unsigned level, std::string_view component,
                   const detail::source_location& loc, actor_id aid,
                   std::string_view fmt, Args&&... args)
    : logger_(ptr),
      event_(log_event::make(level, component, loc, aid, fmt,
                             std::forward<Args>(args)...)),
      fields_(&event_->resource_) {
    // nop
  }

  // -- add fields -------------------------------------------------------------

  /// Adds a boolean or integer field.
  template <class T>
  std::enable_if_t<std::is_integral_v<T>, log_event_sender&&>
  field(std::string_view key, T value) && {
    if (logger_)
      fields_.field(key, value);
    return std::move(*this);
  }

  /// Adds a floating point field.
  log_event_sender&& field(std::string_view key, double value) && {
    if (logger_)
      fields_.field(key, value);
    return std::move(*this);
  }

  /// Adds a string field.
  log_event_sender&& field(std::string_view key, std::string_view value) && {
    if (logger_)
      fields_.field(key, value);
    return std::move(*this);
  }

  /// Adds a formatted string field.
  template <class Arg, class... Args>
  log_event_sender&& field(std::string_view key, std::string_view fmt,
                           Arg&& arg, Args&&... args) && {
    if (logger_)
      fields_.field(key, fmt, std::forward<Arg>(arg),
                    std::forward<Args>(args)...);
    return std::move(*this);
  }

  /// Adds nested fields.
  template <class SubFieldsInitializer>
  auto field(std::string_view key, SubFieldsInitializer&& init)
    -> std::enable_if_t<
      std::is_same_v<decltype(init(std::declval<log_event_fields_builder&>())),
                     void>,
      log_event_sender&&> {
    if (logger_)
      fields_.field(key, std::forward<SubFieldsInitializer>(init));
    return std::move(*this);
  }

  // -- build ------------------------------------------------------------------

  /// Seals the event and passes it to the logger.
  void send() &&;

private:
  [[nodiscard]] resource_type* resource() noexcept {
    return &event_->resource_;
  }

  logger* logger_ = nullptr;

  log_event_ptr event_;

  log_event_fields_builder fields_;
};

} // namespace caf
