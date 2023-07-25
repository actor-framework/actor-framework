// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE save_inspector

#include "caf/save_inspector.hpp"

#include "caf/message.hpp"
#include "caf/serializer.hpp"

#include "core-test.hpp"
#include "inspector-tests.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace caf;

namespace {

struct testee : serializer {
  std::string log;

  size_t indent = 0;

  void set_has_human_readable_format(bool new_value) {
    has_human_readable_format_ = new_value;
  }

  void new_line() {
    log += '\n';
    log.insert(log.end(), indent, ' ');
  }

  bool begin_object(type_id_t, std::string_view object_name) override {
    new_line();
    indent += 2;
    log += "begin object ";
    log.insert(log.end(), object_name.begin(), object_name.end());
    return true;
  }

  bool end_object() override {
    if (indent < 2)
      CAF_FAIL("begin/end mismatch");
    indent -= 2;
    new_line();
    log += "end object";
    return true;
  }

  bool begin_field(std::string_view name) override {
    new_line();
    indent += 2;
    log += "begin field ";
    log.insert(log.end(), name.begin(), name.end());
    return true;
  }

  bool begin_field(std::string_view name, bool) override {
    new_line();
    indent += 2;
    log += "begin optional field ";
    log.insert(log.end(), name.begin(), name.end());
    return true;
  }

  bool begin_field(std::string_view name, span<const type_id_t>,
                   size_t) override {
    new_line();
    indent += 2;
    log += "begin variant field ";
    log.insert(log.end(), name.begin(), name.end());
    return true;
  }

  bool begin_field(std::string_view name, bool, span<const type_id_t>,
                   size_t) override {
    new_line();
    indent += 2;
    log += "begin optional variant field ";
    log.insert(log.end(), name.begin(), name.end());
    return true;
  }

  bool end_field() override {
    if (indent < 2)
      CAF_FAIL("begin/end mismatch");
    indent -= 2;
    new_line();
    log += "end field";
    return true;
  }

  bool begin_tuple(size_t size) override {
    new_line();
    indent += 2;
    log += "begin tuple of size ";
    log += std::to_string(size);
    return true;
  }

  bool end_tuple() override {
    if (indent < 2)
      CAF_FAIL("begin/end mismatch");
    indent -= 2;
    new_line();
    log += "end tuple";
    return true;
  }

  bool begin_key_value_pair() override {
    new_line();
    indent += 2;
    log += "begin key-value pair";
    return true;
  }

  bool end_key_value_pair() override {
    if (indent < 2)
      CAF_FAIL("begin/end mismatch");
    indent -= 2;
    new_line();
    log += "end key-value pair";
    return true;
  }

  bool begin_sequence(size_t size) override {
    new_line();
    indent += 2;
    log += "begin sequence of size ";
    log += std::to_string(size);
    return true;
  }

  bool end_sequence() override {
    if (indent < 2)
      CAF_FAIL("begin/end mismatch");
    indent -= 2;
    new_line();
    log += "end sequence";
    return true;
  }

  bool begin_associative_array(size_t size) override {
    new_line();
    indent += 2;
    log += "begin associative array of size ";
    log += std::to_string(size);
    return true;
  }

  bool end_associative_array() override {
    if (indent < 2)
      CAF_FAIL("begin/end mismatch");
    indent -= 2;
    new_line();
    log += "end associative array";
    return true;
  }

  bool value(std::byte) override {
    new_line();
    log += "byte value";
    return true;
  }

  bool value(bool) override {
    new_line();
    log += "bool value";
    return true;
  }

  bool value(int8_t) override {
    new_line();
    log += "int8_t value";
    return true;
  }

  bool value(uint8_t) override {
    new_line();
    log += "uint8_t value";
    return true;
  }

  bool value(int16_t) override {
    new_line();
    log += "int16_t value";
    return true;
  }

  bool value(uint16_t) override {
    new_line();
    log += "uint16_t value";
    return true;
  }

  bool value(int32_t) override {
    new_line();
    log += "int32_t value";
    return true;
  }

  bool value(uint32_t) override {
    new_line();
    log += "uint32_t value";
    return true;
  }

  bool value(int64_t) override {
    new_line();
    log += "int64_t value";
    return true;
  }

  bool value(uint64_t) override {
    new_line();
    log += "uint64_t value";
    return true;
  }

  bool value(float) override {
    new_line();
    log += "float value";
    return true;
  }

  bool value(double) override {
    new_line();
    log += "double value";
    return true;
  }

  bool value(long double) override {
    new_line();
    log += "long double value";
    return true;
  }

  bool value(std::string_view) override {
    new_line();
    log += "std::string value";
    return true;
  }

  bool value(const std::u16string&) override {
    new_line();
    log += "std::u16string value";
    return true;
  }

  bool value(const std::u32string&) override {
    new_line();
    log += "std::u32string value";
    return true;
  }

  bool value(span<const std::byte>) override {
    new_line();
    log += "byte_span value";
    return true;
  }
};

struct fixture {
  testee f;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(save inspectors can visit C arrays) {
  int32_t xs[] = {1, 2, 3};
  CHECK_EQ(detail::save(f, xs), true);
  CHECK_EQ(f.log, R"_(
begin tuple of size 3
  int32_t value
  int32_t value
  int32_t value
end tuple)_");
}

CAF_TEST(save inspectors can visit simple POD types) {
  point_3d p{1, 1, 1};
  CHECK_EQ(inspect(f, p), true);
  CHECK_EQ(p.x, 1);
  CHECK_EQ(p.y, 1);
  CHECK_EQ(p.z, 1);
  CHECK_EQ(f.log, R"_(
begin object point_3d
  begin field x
    int32_t value
  end field
  begin field y
    int32_t value
  end field
  begin field z
    int32_t value
  end field
end object)_");
}

CAF_TEST(save inspectors can visit node IDs) {
  auto tmp = make_node_id(42, "0102030405060708090A0B0C0D0E0F1011121314");
  auto hash_based_id = unbox(tmp);
  CHECK_EQ(inspect(f, hash_based_id), true);
  CHECK_EQ(f.log, R"_(
begin object caf::node_id
  begin optional variant field data
    begin object caf::hashed_node_id
      begin field process_id
        uint32_t value
      end field
      begin field host
        begin tuple of size 20
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
          uint8_t value
        end tuple
      end field
    end object
  end field
end object)_");
}

CAF_TEST(save inspectors recurse into members) {
  line l{point_3d{1, 1, 1}, point_3d{1, 1, 1}};
  CHECK_EQ(inspect(f, l), true);
  CHECK_EQ(l.p1.x, 1);
  CHECK_EQ(l.p1.y, 1);
  CHECK_EQ(l.p1.z, 1);
  CHECK_EQ(l.p2.x, 1);
  CHECK_EQ(l.p2.y, 1);
  CHECK_EQ(l.p2.z, 1);
  CHECK_EQ(f.log, R"_(
begin object line
  begin field p1
    begin object point_3d
      begin field x
        int32_t value
      end field
      begin field y
        int32_t value
      end field
      begin field z
        int32_t value
      end field
    end object
  end field
  begin field p2
    begin object point_3d
      begin field x
        int32_t value
      end field
      begin field y
        int32_t value
      end field
      begin field z
        int32_t value
      end field
    end object
  end field
end object)_");
}

CAF_TEST(save inspectors support fields with fallbacks and invariants) {
  MESSAGE("save inspectors suppress fields with their default value");
  {
    duration d{"seconds", 12.0};
    CHECK_EQ(inspect(f, d), true);
    CHECK_EQ(d.unit, "seconds");
    CHECK_EQ(d.count, 12.0);
    CHECK_EQ(f.log, R"_(
begin object duration
  begin optional field unit
  end field
  begin field count
    double value
  end field
end object)_");
  }
  f.log.clear();
  MESSAGE("save inspectors include fields with non-default value");
  {
    duration d{"minutes", 42.0};
    CHECK_EQ(inspect(f, d), true);
    CHECK_EQ(d.unit, "minutes");
    CHECK_EQ(d.count, 42.0);
    CHECK_EQ(f.log, R"_(
begin object duration
  begin optional field unit
    std::string value
  end field
  begin field count
    double value
  end field
end object)_");
  }
}

CAF_TEST(save inspectors support optional) {
  std::optional<int32_t> x;
  CHECK_EQ(f.apply(x), true);
  CHECK_EQ(f.log, R"_(
begin object anonymous
  begin optional field value
  end field
end object)_");
}

CAF_TEST(save inspectors support fields with optional values) {
  person p1{"Eduard Example", std::nullopt};
  CHECK_EQ(inspect(f, p1), true);
  CHECK_EQ(f.log, R"_(
begin object person
  begin field name
    std::string value
  end field
  begin optional field phone
  end field
end object)_");
  f.log.clear();
  person p2{"Bruce Almighty", std::string{"776-2323"}};
  CHECK_EQ(inspect(f, p2), true);
  CHECK_EQ(f.log, R"_(
begin object person
  begin field name
    std::string value
  end field
  begin optional field phone
    std::string value
  end field
end object)_");
}

CAF_TEST(save inspectors support fields with getters and setters) {
  foobar fb;
  fb.foo("hello");
  fb.bar("world");
  CHECK(inspect(f, fb));
  CHECK_EQ(fb.foo(), "hello");
  CHECK_EQ(fb.bar(), "world");
  CHECK_EQ(f.log, R"_(
begin object foobar
  begin field foo
    std::string value
  end field
  begin field bar
    std::string value
  end field
end object)_");
}

CAF_TEST(save inspectors support nasty data structures) {
  nasty x;
  CHECK(inspect(f, x));
  CHECK_EQ(f.get_error(), error{});
  CHECK_EQ(f.log, R"_(
begin object nasty
  begin field field_01
    int32_t value
  end field
  begin optional field field_02
  end field
  begin field field_03
    int32_t value
  end field
  begin optional field field_04
  end field
  begin optional field field_05
  end field
  begin optional field field_07
  end field
  begin variant field field_09
    std::string value
  end field
  begin optional variant field field_10
    std::string value
  end field
  begin variant field field_11
    std::string value
  end field
  begin optional variant field field_12
    std::string value
  end field
  begin field field_13
    begin tuple of size 2
      std::string value
      int32_t value
    end tuple
  end field
  begin optional field field_14
  end field
  begin field field_15
    begin tuple of size 2
      std::string value
      int32_t value
    end tuple
  end field
  begin optional field field_16
  end field
  begin field field_17
    int32_t value
  end field
  begin optional field field_18
  end field
  begin field field_19
    int32_t value
  end field
  begin optional field field_20
  end field
  begin optional field field_21
  end field
  begin optional field field_23
  end field
  begin variant field field_25
    std::string value
  end field
  begin optional variant field field_26
    std::string value
  end field
  begin variant field field_27
    std::string value
  end field
  begin optional variant field field_28
    std::string value
  end field
  begin field field_29
    begin tuple of size 2
      std::string value
      int32_t value
    end tuple
  end field
  begin optional field field_30
  end field
  begin field field_31
    begin tuple of size 2
      std::string value
      int32_t value
    end tuple
  end field
  begin optional field field_32
  end field
  begin optional variant field field_33
  end field
  begin optional field field_34
  end field
  begin optional variant field field_35
  end field
  begin optional field field_36
  end field
end object)_");
}

CAF_TEST(save inspectors support all basic STL types) {
  basics x;
  x.v7.emplace("one", 1);
  x.v7.emplace("two", 2);
  x.v7.emplace("three", 3);
  using v8_list = decltype(x.v8);
  using v8_nested_list = v8_list::value_type;
  using array_3i = std::array<int32_t, 3>;
  v8_nested_list v8_1;
  v8_1.emplace_back("hello", array_3i{{1, 2, 3}});
  v8_1.emplace_back("world", array_3i{{2, 3, 4}});
  v8_nested_list v8_2;
  v8_2.emplace_back("foo", array_3i{{0, 0, 0}});
  x.v8.emplace_back(std::move(v8_1));
  x.v8.emplace_back(std::move(v8_2));
  CHECK(inspect(f, x));
  CHECK_EQ(f.log, R"_(
begin object basics
  begin field v1
    begin object anonymous
    end object
  end field
  begin field v2
    int32_t value
  end field
  begin field v3
    begin tuple of size 4
      int32_t value
      int32_t value
      int32_t value
      int32_t value
    end tuple
  end field
  begin field v4
    begin tuple of size 2
      begin object dummy_message
        begin variant field content
          std::string value
        end field
      end object
      begin object dummy_message
        begin variant field content
          std::string value
        end field
      end object
    end tuple
  end field
  begin field v5
    begin tuple of size 2
      int32_t value
      int32_t value
    end tuple
  end field
  begin field v6
    begin tuple of size 2
      int32_t value
      begin object dummy_message
        begin variant field content
          std::string value
        end field
      end object
    end tuple
  end field
  begin field v7
    begin associative array of size 3
      begin key-value pair
        std::string value
        int32_t value
      end key-value pair
      begin key-value pair
        std::string value
        int32_t value
      end key-value pair
      begin key-value pair
        std::string value
        int32_t value
      end key-value pair
    end associative array
  end field
  begin field v8
    begin sequence of size 2
      begin sequence of size 2
        begin tuple of size 2
          std::string value
          begin tuple of size 3
            int32_t value
            int32_t value
            int32_t value
          end tuple
        end tuple
        begin tuple of size 2
          std::string value
          begin tuple of size 3
            int32_t value
            int32_t value
            int32_t value
          end tuple
        end tuple
      end sequence
      begin sequence of size 1
        begin tuple of size 2
          std::string value
          begin tuple of size 3
            int32_t value
            int32_t value
            int32_t value
          end tuple
        end tuple
      end sequence
    end sequence
  end field
end object)_");
}

CAF_TEST(save inspectors support messages) {
  auto x = make_message(1, "two", 3.0);
  MESSAGE("for machine-to-machine formats, messages prefix their types");
  CHECK(inspect(f, x));
  CHECK_EQ(f.log, R"_(
begin object message
  begin field types
    begin sequence of size 3
      uint16_t value
      uint16_t value
      uint16_t value
    end sequence
  end field
  begin field values
    begin tuple of size 3
      int32_t value
      std::string value
      double value
    end tuple
  end field
end object)_");
  MESSAGE("for human-readable formats, messages inline type annotations");
  f.log.clear();
  f.set_has_human_readable_format(true);
  CHECK(inspect(f, x));
  CHECK_EQ(f.log, R"_(
begin sequence of size 3
  int32_t value
  std::string value
  double value
end sequence)_");
}

SCENARIO("save inspectors support apply with a getter and setter") {
  GIVEN("a line object") {
    line x{{10, 10, 10}, {20, 20, 20}};
    WHEN("passing the line to a save inspector with a getter and setter pair") {
      auto get = [&x] { return x; };
      auto set = [&x](line val) { x = val; };
      THEN("the inspector reads the state from the getter") {
        CHECK(f.apply(get, set));
        CHECK_EQ(f.log, R"_(
begin object line
  begin field p1
    begin object point_3d
      begin field x
        int32_t value
      end field
      begin field y
        int32_t value
      end field
      begin field z
        int32_t value
      end field
    end object
  end field
  begin field p2
    begin object point_3d
      begin field x
        int32_t value
      end field
      begin field y
        int32_t value
      end field
      begin field z
        int32_t value
      end field
    end object
  end field
end object)_");
      }
    }
  }
}

SCENARIO("load inspectors support fields with a getter and setter") {
  GIVEN("a person object") {
    auto x = person{"John Doe", {}};
    WHEN("passing a setter and setter pair for the member name") {
      auto get_name = [&x] { return x.name; };
      auto set_name = [&x](std::string val) { x.name = std::move(val); };
      THEN("the inspector reads the state from the getter") {
        CHECK(f.object(x).fields(f.field("name", get_name, set_name),
                                 f.field("phone", x.phone)));
        CHECK_EQ(f.log, R"_(
begin object person
  begin field name
    std::string value
  end field
  begin optional field phone
  end field
end object)_");
      }
    }
  }
}

SCENARIO("save inspectors support std::byte") {
  GIVEN("a struct with std::byte") {
    struct byte_test {
      std::byte v1;
      std::optional<std::byte> v2;
    };
    auto x = byte_test{std::byte{1}, std::byte{2}};
    WHEN("inspecting the struct") {
      THEN("CAF treats std::byte like an unsigned integer") {
        CHECK(f.object(x).fields(f.field("v1", x.v1), f.field("v2", x.v2)));
        CHECK(!f.get_error());
        std::string baseline = R"_(
begin object anonymous
  begin field v1
    byte value
  end field
  begin optional field v2
    byte value
  end field
end object)_";
        CHECK_EQ(f.log, baseline);
      }
    }
  }
}

TEST_CASE("GH-1427 regression") {
  struct opt_test {
    std::optional<int> val;
  };
  auto x = opt_test{};
  CHECK(f.object(x).fields(f.field("val", x.val).fallback(std::nullopt)));
  CHECK(!f.get_error());
  std::string baseline = R"_(
begin object anonymous
  begin optional field val
  end field
end object)_";
  CHECK_EQ(f.log, baseline);
}

END_FIXTURE_SCOPE()
