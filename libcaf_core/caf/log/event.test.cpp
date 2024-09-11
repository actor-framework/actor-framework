// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/log/event.hpp"

#include "caf/test/test.hpp"

#include "caf/log/level.hpp"
#include "caf/logger.hpp"
#include "caf/ref_counted.hpp"

using namespace caf;
using namespace std::literals;

namespace {

template <class T>
std::pair<std::string_view, T>
get_at(size_t index, const log::event::field_list& fields) {
  auto i = fields.begin();
  if (i == fields.end())
    CAF_RAISE_ERROR(std::logic_error, "fields list is empty");
  for (size_t j = 0; j < index; ++j)
    if (++i == fields.end())
      CAF_RAISE_ERROR(std::logic_error, "index out of range");
  return {i->key, std::get<T>(i->value)};
}

// A trivial logger implementation that stores the last event.
class mock_logger : public logger, public ref_counted {
public:
  log::event_ptr event;

  void ref_logger() const noexcept override {
    ref();
  }

  void deref_logger() const noexcept override {
    deref();
  }

  bool accepts(unsigned, std::string_view) override {
    return true;
  }

private:
  void do_log(log::event_ptr&& ptr) override {
    event = std::move(ptr);
  }

  void init(const actor_system_config&) override {
    // nop
  }

  void start() override {
    // nop
  }

  void stop() override {
    // nop
  }
};

std::pair<std::string_view, std::string>
de_chunk(const std::pair<std::string_view, chunked_string>& x) {
  return {x.first, to_string(x.second)};
}

} // namespace

TEST("fields builder") {
  auto init_sub_fields = [](auto& builder) {
    auto init_sub_sub_fields = [](auto& builder) {
      builder.field("foo-7-3-1", 3);
      builder.field("foo-7-3-2", 4);
    };
    builder.field("foo-7-1", 1);
    builder.field("foo-7-2", 2);
    builder.field("foo-7-3", init_sub_sub_fields);
  };
  auto resource = detail::monotonic_buffer_resource{};
  auto fields = log::event_fields_builder{&resource}
                  .field("foo-1", true)
                  .field("foo-2", 23)
                  .field("foo-3", 42u)
                  .field("foo-4", 2.0)
                  .field("foo-5", "bar")
                  .field("foo-6", "{}, {}!", "Hello", "World")
                  .field("foo-7", init_sub_fields)
                  .build();
  check_eq(get_at<bool>(0, fields), std::pair{"foo-1"sv, true});
  check_eq(get_at<int64_t>(1, fields), std::pair{"foo-2"sv, int64_t{23}});
  check_eq(get_at<uint64_t>(2, fields), std::pair{"foo-3"sv, uint64_t{42}});
  check_eq(get_at<double>(3, fields), std::pair{"foo-4"sv, 2.0});
  check_eq(get_at<std::string_view>(4, fields), std::pair{"foo-5"sv, "bar"sv});
  check_eq(de_chunk(get_at<chunked_string>(5, fields)),
           std::pair{"foo-6"sv, "Hello, World!"s});
  auto [foo_7, foo_7_fields] = get_at<log::event::field_list>(6, fields);
  check_eq(foo_7, "foo-7"sv);
  check_eq(get_at<int64_t>(0, foo_7_fields),
           std::pair{"foo-7-1"sv, int64_t{1}});
  check_eq(get_at<int64_t>(1, foo_7_fields),
           std::pair{"foo-7-2"sv, int64_t{2}});
  auto [foo_7_3, foo_7_3_fields] = get_at<log::event::field_list>(2,
                                                                  foo_7_fields);
  check_eq(foo_7_3, "foo-7-3"sv);
  check_eq(get_at<int64_t>(0, foo_7_3_fields),
           std::pair{"foo-7-3-1"sv, int64_t{3}});
  check_eq(get_at<int64_t>(1, foo_7_3_fields),
           std::pair{"foo-7-3-2"sv, int64_t{4}});
}

TEST("log event sender") {
  auto loc = detail::source_location::current();
  auto mlog = mock_logger{};
  SECTION("trivial message") {
    log::event_sender(&mlog, log::level::debug, "foo", loc, 0, "hello")
      .field("foo", 42)
      .field("bar", "baz")
      .send();
    auto event = mlog.event;
    if (!check_ne(event, nullptr))
      return;
    check_eq(event->level(), log::level::debug);
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
    log::event_sender(&mlog, log::level::debug, "foo", loc, 123, "{}, {}!",
                      "Hello", "World")
      .field("foo", 42)
      .field("bar", "baz")
      .send();
    auto event = mlog.event;
    if (!check_ne(event, nullptr))
      return;
    check_eq(event->level(), log::level::debug);
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

TEST("with_message") {
  auto loc = detail::source_location::current();
  auto mlog = mock_logger{};
  log::event_sender(&mlog, log::level::debug, "foo", loc, 0, "message 1")
    .field("foo", 42)
    .field("str", "{}, {}!", "Hello", "World")
    .field("nested", [](auto& sub) { sub.field("key-1", 1).field("key-2", 2); })
    .send();
  auto event_1 = mlog.event;
  auto event_2 = event_1->with_message("message 2", log::keep_timestamp);
  check_eq(to_string(event_1->message()), "message 1"sv);
  check_eq(to_string(event_2->message()), "message 2"sv);
  auto fields_1 = event_1->fields();
  auto fields_1_3 = get_at<log::event::field_list>(2, fields_1).second;
  auto fields_2 = event_2->fields();
  auto fields_2_3 = get_at<log::event::field_list>(2, fields_2).second;
  check_eq(get_at<int64_t>(0, fields_1), std::pair{"foo"sv, int64_t{42}});
  check_eq(de_chunk(get_at<chunked_string>(1, fields_1)),
           std::pair{"str"sv, "Hello, World!"s});
  check_eq(get_at<int64_t>(0, fields_1_3), std::pair{"key-1"sv, int64_t{1}});
  check_eq(get_at<int64_t>(1, fields_1_3), std::pair{"key-2"sv, int64_t{2}});
  check_eq(get_at<int64_t>(0, fields_2), std::pair{"foo"sv, int64_t{42}});
  check_eq(de_chunk(get_at<chunked_string>(1, fields_2)),
           std::pair{"str"sv, "Hello, World!"s});
  check_eq(get_at<int64_t>(0, fields_2_3), std::pair{"key-1"sv, int64_t{1}});
  check_eq(get_at<int64_t>(1, fields_2_3), std::pair{"key-2"sv, int64_t{2}});
  check_eq(event_1->level(), event_2->level());
  check_eq(event_1->component(), event_2->component());
  check_eq(event_1->line_number(), event_2->line_number());
  check_eq(event_1->file_name(), event_2->file_name());
  check_eq(event_1->function_name(), event_2->function_name());
  check_eq(event_1->actor_id(), event_2->actor_id());
  check_eq(event_1->thread_id(), event_2->thread_id());
  check_eq(event_1->timestamp(), event_2->timestamp());
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  auto event_3 = event_2->with_message("message 2");
  check_ne(event_2->timestamp(), event_3->timestamp());
}
