// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/log_event.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

namespace {

template <class T>
std::pair<std::string_view, T>
get_at(size_t index, const log_event::field_list& fields) {
  auto i = fields.begin();
  if (i == fields.end())
    CAF_RAISE_ERROR(std::logic_error, "fields list is empty");
  for (size_t j = 0; j < index; ++j)
    if (++i == fields.end())
      CAF_RAISE_ERROR(std::logic_error, "index out of range");
  return {i->key, std::get<T>(i->value)};
}

std::pair<std::string_view, std::string>
de_chunk(const std::pair<std::string_view, chunked_string>& x) {
  return {x.first, to_string(x.second)};
}

TEST("fields builder") {
  auto init_sub_fields = [](auto& builder) {
    auto init_sub_sub_fields = [](auto& builder) {
      builder.add_field("foo-7-3-1", 3);
      builder.add_field("foo-7-3-2", 4);
    };
    builder.add_field("foo-7-1", 1);
    builder.add_field("foo-7-2", 2);
    builder.add_field("foo-7-3", init_sub_sub_fields);
  };
  auto resource = detail::monotonic_buffer_resource{};
  auto builder = log_event_fields_builder{&resource};
  builder.add_field("foo-1", true);
  builder.add_field("foo-2", 23);
  builder.add_field("foo-3", 42u);
  builder.add_field("foo-4", 2.0);
  builder.add_field("foo-5", "bar");
  builder.add_field("foo-6", "{}, {}!", "Hello", "World");
  builder.add_field("foo-7", init_sub_fields);
  auto fields = builder.build();
  check_eq(get_at<bool>(0, fields), std::pair{"foo-1"sv, true});
  check_eq(get_at<int64_t>(1, fields), std::pair{"foo-2"sv, int64_t{23}});
  check_eq(get_at<uint64_t>(2, fields), std::pair{"foo-3"sv, uint64_t{42}});
  check_eq(get_at<double>(3, fields), std::pair{"foo-4"sv, 2.0});
  check_eq(get_at<std::string_view>(4, fields), std::pair{"foo-5"sv, "bar"sv});
  check_eq(de_chunk(get_at<chunked_string>(5, fields)),
           std::pair{"foo-6"sv, "Hello, World!"s});
  auto [foo_7, foo_7_fields] = get_at<log_event::field_list>(6, fields);
  check_eq(foo_7, "foo-7"sv);
  check_eq(get_at<int64_t>(0, foo_7_fields),
           std::pair{"foo-7-1"sv, int64_t{1}});
  check_eq(get_at<int64_t>(1, foo_7_fields),
           std::pair{"foo-7-2"sv, int64_t{2}});
  auto [foo_7_3, foo_7_3_fields] = get_at<log_event::field_list>(2,
                                                                 foo_7_fields);
  check_eq(foo_7_3, "foo-7-3"sv);
  check_eq(get_at<int64_t>(0, foo_7_3_fields),
           std::pair{"foo-7-3-1"sv, int64_t{3}});
  check_eq(get_at<int64_t>(1, foo_7_3_fields),
           std::pair{"foo-7-3-2"sv, int64_t{4}});
}

TEST("log event builder") {
  auto loc = detail::source_location::current();
  SECTION("trivial message") {
    auto event = log_event_builder(CAF_LOG_LEVEL_DEBUG, "foo", loc, 0, "hello")
                   .add_field("foo", 42)
                   .add_field("bar", "baz")
                   .build();
    check_eq(event->level(), unsigned{CAF_LOG_LEVEL_DEBUG});
    check_eq(event->component(), "foo"sv);
    check_eq(event->line_number(), loc.line());
    check_eq(event->file_name(), loc.file_name());
    check_eq(event->function_name(), loc.function_name());
    check_eq(event->actor_id(), 0u);
    check_eq(to_string(event->message()), "hello"sv);
    auto fields = event->fields();
    check_eq(get_at<int64_t>(0, fields), std::pair{"foo"sv, int64_t{42}});
    check_eq(get_at<std::string_view>(1, fields), std::pair{"bar"sv, "baz"sv});
  }
  SECTION("formatted message") {
    auto event = log_event_builder(CAF_LOG_LEVEL_DEBUG, "foo", loc, 123,
                                   "{}, {}!", "Hello", "World")
                   .add_field("foo", 42)
                   .add_field("bar", "baz")
                   .build();
    check_eq(event->level(), unsigned{CAF_LOG_LEVEL_DEBUG});
    check_eq(event->component(), "foo"sv);
    check_eq(event->line_number(), loc.line());
    check_eq(event->file_name(), loc.file_name());
    check_eq(event->function_name(), loc.function_name());
    check_eq(event->actor_id(), 123u);
    check_eq(to_string(event->message()), "Hello, World!"sv);
    auto fields = event->fields();
    check_eq(get_at<int64_t>(0, fields), std::pair{"foo"sv, int64_t{42}});
    check_eq(get_at<std::string_view>(1, fields), std::pair{"bar"sv, "baz"sv});
  }
}

} // namespace
