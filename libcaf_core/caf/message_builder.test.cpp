// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/message_builder.hpp"

#include "caf/test/test.hpp"

#include "caf/init_global_meta_objects.hpp"
#include "caf/log/test.hpp"
#include "caf/type_id.hpp"

using namespace caf;

class counted_int : public ref_counted {
public:
  explicit counted_int() : value_(0) {
    // nop
  }

  explicit counted_int(int initial_value) : value_(initial_value) {
    // nop
  }

  int value() const {
    return value_;
  }

  void value(int new_value) {
    value_ = new_value;
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, counted_int& x) {
    return f.object(x).fields(f.field("value", x.value_));
  }

private:
  int value_;
};

using counted_int_ptr = intrusive_ptr<counted_int>;

template <class Inspector>
bool inspect(Inspector& f, counted_int_ptr& x) {
  if constexpr (Inspector::is_loading) {
    if (!x)
      x = make_counted<counted_int>(0);
    return inspect(f, *x);
  } else {
    if (!x) {
      auto tmp = counted_int{0};
      return inspect(f, tmp);
    }
    return inspect(f, *x);
  }
}

CAF_BEGIN_TYPE_ID_BLOCK(message_builder_test, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(message_builder_test, (counted_int))
  CAF_ADD_TYPE_ID(message_builder_test, (counted_int_ptr))

CAF_END_TYPE_ID_BLOCK(message_builder_test)

namespace {

TEST("message_builder can build messages incrementally") {
  message_builder builder;
  SECTION("a default-constructed builder is empty") {
    check(builder.empty());
    check(builder.to_message().empty());
    check_eq(builder.size(), 0u);
  }
  SECTION("append() adds values to a builder") {
    log::test::debug("after adding 1, the message is (1)");
    {
      builder.append(int32_t{1});
      auto msg = builder.to_message();
      check_eq(builder.size(), 1u);
      check_eq(msg.types(), make_type_id_list<int32_t>());
      check_eq(msg.get_as<int32_t>(0), 1);
    }
    log::test::debug("after adding [2, 3], the message is (1, 2, 3)");
    {
      std::vector<int32_t> xs{2, 3};
      builder.append(xs.begin(), xs.end());
      check_eq(builder.size(), 3u);
      auto msg = builder.to_message();
      check_eq(msg.types(), make_type_id_list<int32_t, int32_t, int32_t>());
      check_eq(msg.get_as<int32_t>(0), 1);
      check_eq(msg.get_as<int32_t>(1), 2);
      check_eq(msg.get_as<int32_t>(2), 3);
    }
  }
  SECTION("append_from() adds values from another message") {
    std::vector<int32_t> xs{1, 2, 3};
    builder.append(xs.begin(), xs.end());
    auto msg = builder.to_message();
    message_builder builder2;
    check(builder2.empty());
    check(builder2.to_message().empty());
    SECTION("builder appends the values when first < msg.size()") {
      builder2.append_from(msg, 0, msg.size());
      auto msg2 = builder2.to_message();
      if (check_eq(msg.types(),
                   make_type_id_list<int32_t, int32_t, int32_t>())) {
        check_eq(msg.get_as<int32_t>(0), 1);
        check_eq(msg.get_as<int32_t>(1), 2);
        check_eq(msg.get_as<int32_t>(2), 3);
      }
    }
    SECTION("builder does nothing when first >= msg.size()") {
      builder2.append_from(msg, 4, msg.size());
      auto msg2 = builder2.to_message();
      check_eq(msg2.size(), 0u);
    }
    SECTION("builder appends the values from first index to msg.size()") {
      builder2.append_from(msg, 2, msg.size());
      auto msg2 = builder2.to_message();
      check_eq(msg2.size(), 1u);
      check_eq(msg2.get_as<int32_t>(0), 3);
    }
  }
  SECTION("to_message copies all elements into a message") {
    builder.append("foo");
    builder.append(make_counted<counted_int>(42));
    check_eq(builder.size(), 2u);
    auto msg = builder.to_message();
    if (check_eq(msg.types(),
                 make_type_id_list<std::string, counted_int_ptr>())) {
      check_eq(msg.get_as<std::string>(0), "foo");
      const auto& ci = msg.get_as<counted_int_ptr>(1);
      check_eq(ci->value(), 42);
      check_eq(ci->get_reference_count(), 2u);
    }
  }
  SECTION("clear() removes all messages from the builder") {
    const auto vec = std::vector<int32_t>{1, 2, 3};
    builder.append(vec.begin(), vec.end());
    check_eq(builder.size(), 3u);
    builder.clear();
    check(builder.empty());
    check(builder.to_message().empty());
    check_eq(builder.size(), 0u);
  }
}

TEST("move_to_message move-constructs message elements") {
  message_builder builder;
  SECTION("move_to_message copies primitive values") {
    builder.append(1);
    builder.append(2);
    auto msg = builder.move_to_message();
    if (check_eq(msg.types(), make_type_id_list<int32_t, int32_t>())) {
      check_eq(msg.get_as<int32_t>(0), 1);
      check_eq(msg.get_as<int32_t>(1), 2);
    }
  }
  SECTION("move_to_message moves movable values") {
    builder.append("foo");
    builder.append(make_counted<counted_int>(42));
    auto msg = builder.move_to_message();
    if (check_eq(msg.types(),
                 make_type_id_list<std::string, counted_int_ptr>())) {
      check_eq(msg.get_as<std::string>(0), "foo");
      const auto& ci = msg.get_as<counted_int_ptr>(1);
      check_eq(ci->value(), 42);
      check_eq(ci->get_reference_count(), 1u);
    }
  }
  SECTION("move_to_message moves value from message with no other reference") {
    builder.append_from(make_message(make_counted<counted_int>(42)), 0, 1);
    auto msg = builder.move_to_message();
    if (check_eq(msg.types(), make_type_id_list<counted_int_ptr>())) {
      const auto& ci = msg.get_as<counted_int_ptr>(0);
      check_eq(ci->value(), 42);
      check_eq(ci->get_reference_count(), 1u);
    }
  }
  SECTION("move_to_message copies value from message with multiple reference") {
    builder.append_from(make_message(make_counted<counted_int>(1),
                                     make_counted<counted_int>(2), 3),
                        0, 3);
    auto msg = builder.move_to_message();
    if (check_eq(
          msg.types(),
          make_type_id_list<counted_int_ptr, counted_int_ptr, int32_t>())) {
      const auto& ci = msg.get_as<counted_int_ptr>(0);
      check_eq(ci->value(), 1);
      check_eq(ci->get_reference_count(), 2u);
      check_eq(msg.get_as<int32_t>(2), 3);
    }
  }
}

TEST("message_builder can build messages from iterator pairs") {
  std::vector<int32_t> xs{1, 2, 3};
  message_builder builder(xs.begin(), xs.end());
  check(!builder.empty());
  check_eq(builder.size(), 3u);
  auto msg = builder.to_message();
  check_eq(msg.types(), make_type_id_list<int32_t, int32_t, int32_t>());
  check_eq(msg.get_as<int32_t>(0), 1);
  check_eq(msg.get_as<int32_t>(1), 2);
  check_eq(msg.get_as<int32_t>(2), 3);
}

TEST_INIT() {
  init_global_meta_objects<id_block::message_builder_test>();
}

} // namespace
