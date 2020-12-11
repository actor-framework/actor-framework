/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE load_inspector

#include "caf/load_inspector.hpp"

#include "caf/test/dsl.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "caf/load_inspector_base.hpp"
#include "caf/message.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"
#include "caf/variant.hpp"

#include "inspector-tests.hpp"

using namespace caf;

namespace {

struct testee : deserializer {
  std::string log;

  bool load_field_failed(string_view, sec code) {
    set_error(make_error(code));
    return false;
  }

  size_t indent = 0;

  void new_line() {
    log += '\n';
    log.insert(log.end(), indent, ' ');
  }

  bool fetch_next_object_type(type_id_t&) override {
    return false;
  }

  bool begin_object(type_id_t, string_view object_name) override {
    new_line();
    indent += 2;
    log += "begin object ";
    log.insert(log.end(), object_name.begin(), object_name.end());
    return true;
  }

  bool end_object() override {
    indent -= 2;
    new_line();
    log += "end object";
    return true;
  }

  bool begin_field(string_view name) override {
    new_line();
    indent += 2;
    log += "begin field ";
    log.insert(log.end(), name.begin(), name.end());
    return true;
  }

  bool begin_field(string_view name, bool& is_present) override {
    new_line();
    indent += 2;
    log += "begin optional field ";
    log.insert(log.end(), name.begin(), name.end());
    is_present = false;
    return true;
  }

  bool begin_field(string_view name, span<const type_id_t>,
                   size_t& type_index) override {
    new_line();
    indent += 2;
    log += "begin variant field ";
    log.insert(log.end(), name.begin(), name.end());
    type_index = 0;
    return true;
  }

  bool begin_field(string_view name, bool& is_present, span<const type_id_t>,
                   size_t&) override {
    new_line();
    indent += 2;
    log += "begin optional variant field ";
    log.insert(log.end(), name.begin(), name.end());
    is_present = false;
    return true;
  }

  bool end_field() override {
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
    indent -= 2;
    new_line();
    log += "end key-value pair";
    return true;
  }

  bool begin_sequence(size_t& size) override {
    size = 0;
    new_line();
    indent += 2;
    log += "begin sequence of size ";
    log += std::to_string(size);
    return true;
  }

  bool end_sequence() override {
    indent -= 2;
    new_line();
    log += "end sequence";
    return true;
  }

  bool begin_associative_array(size_t& size) override {
    size = 0;
    new_line();
    indent += 2;
    log += "begin associative array of size ";
    log += std::to_string(size);
    return true;
  }

  bool end_associative_array() override {
    indent -= 2;
    new_line();
    log += "end associative array";
    return true;
  }

  bool value(bool& x) override {
    new_line();
    log += "bool value";
    x = false;
    return true;
  }

  template <class T>
  bool primitive_value(T& x) {
    new_line();
    auto tn = type_name_v<T>;
    log.insert(log.end(), tn.begin(), tn.end());
    log += " value";
    x = T{};
    return true;
  }

  bool value(byte& x) override {
    new_line();
    log += "byte value";
    x = byte{};
    return true;
  }

  bool value(int8_t& x) override {
    return primitive_value(x);
  }

  bool value(uint8_t& x) override {
    return primitive_value(x);
  }

  bool value(int16_t& x) override {
    return primitive_value(x);
  }

  bool value(uint16_t& x) override {
    return primitive_value(x);
  }

  bool value(int32_t& x) override {
    return primitive_value(x);
  }

  bool value(uint32_t& x) override {
    return primitive_value(x);
  }

  bool value(int64_t& x) override {
    return primitive_value(x);
  }

  bool value(uint64_t& x) override {
    return primitive_value(x);
  }

  bool value(float& x) override {
    return primitive_value(x);
  }

  bool value(double& x) override {
    return primitive_value(x);
  }

  bool value(long double& x) override {
    return primitive_value(x);
  }

  bool value(std::string& x) override {
    return primitive_value(x);
  }

  bool value(std::u16string& x) override {
    return primitive_value(x);
  }

  bool value(std::u32string& x) override {
    return primitive_value(x);
  }

  bool value(span<byte> xs) override {
    new_line();
    log += "caf::span<caf::byte> value";
    for (auto& x : xs)
      x = byte{0};
    return true;
  }
};

struct fixture {
  testee f;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(load_inspector_tests, fixture)

CAF_TEST(load inspectors can visit simple POD types) {
  point_3d p{1, 1, 1};
  CAF_CHECK_EQUAL(inspect(f, p), true);
  CAF_CHECK_EQUAL(p.x, 0);
  CAF_CHECK_EQUAL(p.y, 0);
  CAF_CHECK_EQUAL(p.z, 0);
  CAF_CHECK_EQUAL(f.log, R"_(
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

CAF_TEST(load inspectors recurse into members) {
  line l{point_3d{1, 1, 1}, point_3d{1, 1, 1}};
  CAF_CHECK_EQUAL(inspect(f, l), true);
  CAF_CHECK_EQUAL(l.p1.x, 0);
  CAF_CHECK_EQUAL(l.p1.y, 0);
  CAF_CHECK_EQUAL(l.p1.z, 0);
  CAF_CHECK_EQUAL(l.p2.x, 0);
  CAF_CHECK_EQUAL(l.p2.y, 0);
  CAF_CHECK_EQUAL(l.p2.z, 0);
  CAF_CHECK_EQUAL(f.log, R"_(
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

CAF_TEST(load inspectors support optional) {
  optional<int32_t> x;
  CAF_CHECK_EQUAL(f.apply(x), true);
  CAF_CHECK_EQUAL(f.log, R"_(
begin object anonymous
  begin optional field value
  end field
end object)_");
}

CAF_TEST(load inspectors support fields with fallbacks and invariants) {
  duration d{"minutes", 42};
  CAF_CHECK_EQUAL(inspect(f, d), true);
  CAF_CHECK_EQUAL(d.unit, "seconds");
  CAF_CHECK_EQUAL(d.count, 0.0);
  CAF_CHECK_EQUAL(f.log, R"_(
begin object duration
  begin optional field unit
  end field
  begin field count
    double value
  end field
end object)_");
}

CAF_TEST(load inspectors support fields with optional values) {
  person p{"Bruce Almighty", std::string{"776-2323"}};
  CAF_CHECK_EQUAL(inspect(f, p), true);
  CAF_CHECK_EQUAL(p.name, "");
  CAF_CHECK_EQUAL(p.phone, none);
  CAF_CHECK_EQUAL(f.log, R"_(
begin object person
  begin field name
    std::string value
  end field
  begin optional field phone
  end field
end object)_");
}

CAF_TEST(load inspectors support fields with getters and setters) {
  foobar fb;
  fb.foo("hello");
  fb.bar("world");
  CAF_CHECK_EQUAL(inspect(f, fb), true);
  CAF_CHECK_EQUAL(fb.foo(), "");
  CAF_CHECK_EQUAL(fb.bar(), "");
  CAF_CHECK_EQUAL(f.log, R"_(
begin object foobar
  begin field foo
    std::string value
  end field
  begin field bar
    std::string value
  end field
end object)_");
}

CAF_TEST(load inspectors support variant fields) {
  dummy_message d;
  d.content = 42.0;
  CAF_CHECK(inspect(f, d));
  // Our dummy inspector resets variants to their first type.
  CAF_CHECK(holds_alternative<std::string>(d.content));
  CAF_CHECK_EQUAL(f.log, R"_(
begin object dummy_message
  begin variant field content
    std::string value
  end field
end object)_");
}

CAF_TEST(load inspectors support variant fields with fallbacks) {
  fallback_dummy_message d;
  d.content = std::string{"hello world"};
  CAF_CHECK(inspect(f, d));
  CAF_CHECK_EQUAL(d.content, 42.0);
  CAF_CHECK_EQUAL(f.log, R"_(
begin object fallback_dummy_message
  begin optional variant field content
  end field
end object)_");
}

CAF_TEST(load inspectors support nasty data structures) {
  nasty x;
  CAF_CHECK(inspect(f, x));
  CAF_CHECK_EQUAL(f.log, R"_(
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
  end field
  begin variant field field_11
    std::string value
  end field
  begin optional variant field field_12
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
  end field
  begin variant field field_27
    std::string value
  end field
  begin optional variant field field_28
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

CAF_TEST(load inspectors support all basic STL types) {
  basics x;
  CAF_CHECK(inspect(f, x));
  CAF_CHECK_EQUAL(f.log, R"_(
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
    begin associative array of size 0
    end associative array
  end field
  begin field v8
    begin sequence of size 0
    end sequence
  end field
end object)_");
}

CAF_TEST(load inspectors support messages) {
  auto msg = make_message(1, "two", 3.0);
}

CAF_TEST_FIXTURE_SCOPE_END()
