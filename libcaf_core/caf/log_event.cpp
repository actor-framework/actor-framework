// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/log_event.hpp"

#include "caf/make_counted.hpp"

#include <new>

namespace caf {

namespace {

template <class T>
using allocator_t = detail::monotonic_buffer_resource::allocator<T>;

std::string_view deep_copy_impl(detail::monotonic_buffer_resource* resource,
                                std::string_view str) {
  auto* buf = allocator_t<char>{resource}.allocate(str.size());
  memcpy(buf, str.data(), str.size());
  return std::string_view{buf, str.size()};
}

} // namespace

log_event_ptr log_event::make(unsigned level, std::string_view component,
                              const detail::source_location& loc,
                              std::string_view msg) {
  using chunk_node = chunked_string::node_type;
  auto event = make(level, component, loc);
  auto* res = &event->resource_;
  auto* buf = allocator_t<chunk_node>{res}.allocate(1);
  auto* node = new (buf) chunk_node{deep_copy_impl(res, msg), nullptr};
  event->message_ = chunked_string{node};
  return event;
}

log_event_ptr log_event::make(unsigned level, std::string_view component,
                              const detail::source_location& loc) {
  return make_counted<log_event>(level, component, loc);
}

log_event_fields_builder::log_event_fields_builder(
  resource_type* resource) noexcept {
  new (&fields_) list_type(resource);
}

std::string_view log_event_fields_builder::deep_copy(std::string_view str) {
  auto* buf = allocator_t<char>{resource()}.allocate(str.size());
  memcpy(buf, str.data(), str.size());
  return std::string_view{buf, str.size()};
}

intrusive_ptr<log_event> log_event_builder::build() && {
  event_->first_field_ = fields_.build().head;
  return std::move(event_);
}

} // namespace caf
