// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/type_id_list.hpp"

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/detail/type_id_list_builder.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"
#include "caf/message.hpp"
#include "caf/policy/use_type_names.hpp"
#include "caf/sec.hpp"
#include "caf/type_id.hpp"

#include <string>

namespace detail {

struct my_secret {
  int value;
};

template <class Inspector>
bool inspect(Inspector& f, my_secret& x) {
  return f.object(x).fields(f.field("value", x.value));
}

} // namespace detail

namespace io {

struct protocol {
  std::string name;
};

template <class Inspector>
bool inspect(Inspector& f, protocol& x) {
  return f.object(x).fields(f.field("name", x.name));
}

} // namespace io

// A type ID block with types that live in namespaces that also exist as nested
// CAF namespaces. This is a regression test for
// https://github.com/actor-framework/actor-framework/issues/1195. We don't need
// to actually use these types, only check whether the code compiles.

CAF_BEGIN_TYPE_ID_BLOCK(type_id_test, caf::first_custom_type_id + 120, 10)

  CAF_ADD_TYPE_ID(type_id_test, (detail::my_secret))
  CAF_ADD_TYPE_ID(type_id_test, (io::protocol))

CAF_END_TYPE_ID_BLOCK(type_id_test)

using namespace caf;

TEST("lists store the size at index 0") {
  type_id_t data[] = {type_id_t{3}, type_id_t{1}, type_id_t{2}, type_id_t{4}};
  type_id_list xs{data};
  check_eq(xs.size(), 3u);
  check_eq(xs[0], type_id_t{1});
  check_eq(xs[1], type_id_t{2});
  check_eq(xs[2], type_id_t{4});
}

TEST("lists are comparable") {
  type_id_t data[] = {type_id_t{3}, type_id_t{1}, type_id_t{2}, type_id_t{4}};
  type_id_list xs{data};
  type_id_t data_copy[] = {type_id_t{3}, type_id_t{1}, type_id_t{2},
                           type_id_t{4}};
  type_id_list ys{data_copy};
  check_eq(xs, ys);
  data_copy[1] = type_id_t{10};
  check_ne(xs, ys);
  check_lt(xs, ys);
  check_eq(make_type_id_list<add_atom>(), make_type_id_list<add_atom>());
  check_ne(make_type_id_list<add_atom>(), make_type_id_list<ok_atom>());
}

TEST("make_type_id_list constructs a list from types") {
  auto xs = make_type_id_list<uint8_t, bool, float>();
  check_eq(xs.size(), 3u);
  check_eq(xs[0], type_id_v<uint8_t>);
  check_eq(xs[1], type_id_v<bool>);
  check_eq(xs[2], type_id_v<float>);
}

TEST("type ID lists are convertible to strings") {
  auto xs = make_type_id_list<uint16_t, bool, float, long double>();
  check_eq(to_string(xs), "[uint16_t, bool, float, ldouble]");
}

TEST("type ID lists are concatenable") {
  // 1 + 0
  check_eq((make_type_id_list<int8_t>()),
           type_id_list::concat(make_type_id_list<int8_t>(),
                                make_type_id_list()));
  check_eq((make_type_id_list<int8_t>()),
           type_id_list::concat(make_type_id_list(),
                                make_type_id_list<int8_t>()));
  // 1 + 1
  check_eq((make_type_id_list<int8_t, int16_t>()),
           type_id_list::concat(make_type_id_list<int8_t>(),
                                make_type_id_list<int16_t>()));
  // 2 + 0
  check_eq((make_type_id_list<int8_t, int16_t>()),
           type_id_list::concat(make_type_id_list<int8_t, int16_t>(),
                                make_type_id_list()));
  check_eq((make_type_id_list<int8_t, int16_t>()),
           type_id_list::concat(make_type_id_list(),
                                make_type_id_list<int8_t, int16_t>()));
  // 2 + 1
  check_eq((make_type_id_list<int8_t, int16_t, int32_t>()),
           type_id_list::concat(make_type_id_list<int8_t, int16_t>(),
                                make_type_id_list<int32_t>()));
  check_eq((make_type_id_list<int8_t, int16_t, int32_t>()),
           type_id_list::concat(make_type_id_list<int8_t>(),
                                make_type_id_list<int16_t, int32_t>()));
  // 2 + 2
  check_eq((make_type_id_list<int8_t, int16_t, int32_t, int64_t>()),
           type_id_list::concat(make_type_id_list<int8_t, int16_t>(),
                                make_type_id_list<int32_t, int64_t>()));
}

SCENARIO("type ID lists are serializable") {
  GIVEN("a non-empty type ID list") {
    auto xs = make_type_id_list<int32_t, std::string, double>();
    WHEN("serializing with a binary serializer") {
      byte_buffer buf;
      binary_serializer sink{buf};
      check(sink.value(xs));
      THEN("a binary deserializer reproduces the list") {
        binary_deserializer source{buf};
        type_id_list ys = make_type_id_list();
        check(source.value(ys));
        check_eq(xs, ys);
      }
      AND_THEN("apply roundtrips via the binary serializer") {
        binary_deserializer source{buf};
        type_id_list ys = make_type_id_list();
        check(source.apply(ys));
        check_eq(xs, ys);
      }
      AND_THEN("a deserializer with policy::use_type_names fails") {
        binary_deserializer source{buf, policy::use_type_names};
        type_id_list ys = make_type_id_list();
        check(!source.value(ys));
      }
    }
    WHEN("serializing with policy::use_type_names") {
      byte_buffer buf;
      binary_serializer sink{buf, policy::use_type_names};
      check(sink.value(xs));
      THEN("a matching binary deserializer reproduces the list") {
        binary_deserializer source{buf, policy::use_type_names};
        type_id_list ys = make_type_id_list();
        check(source.value(ys));
        check_eq(xs, ys);
      }
      AND_THEN("a deserializer without the flag does not reproduce the list") {
        binary_deserializer source{buf};
        type_id_list ys = make_type_id_list();
        auto ok = source.value(ys);
        check(!ok || ys != xs);
      }
    }
    WHEN("serializing with policy::use_type_names and a custom mapper") {
      struct alias_mapper : type_id_mapper {
        std::string_view operator()(type_id_t type) const override {
          if (type == type_id_v<int32_t>)
            return "my_app.int32";
          return query_type_name(type);
        }
        type_id_t operator()(std::string_view name) const override {
          if (name == "my_app.int32")
            return type_id_v<int32_t>;
          return query_type_id(name);
        }
      };
      alias_mapper mapper;
      byte_buffer buf;
      binary_serializer sink{buf, policy::use_type_names};
      sink.mapper(&mapper);
      check(sink.value(xs));
      THEN("a deserializer with the same mapper reproduces the list") {
        binary_deserializer source{buf, policy::use_type_names};
        source.mapper(&mapper);
        type_id_list ys = make_type_id_list();
        check(source.value(ys));
        check_eq(xs, ys);
      }
      AND_THEN("a deserializer without the mapper fails") {
        binary_deserializer source{buf, policy::use_type_names};
        type_id_list ys = make_type_id_list();
        check(!source.value(ys));
      }
    }
    WHEN("using a mapper that does not fall back to global queries") {
      struct alias_mapper : type_id_mapper {
        std::string_view operator()(type_id_t type) const override {
          if (type == type_id_v<int32_t>)
            return "my_app.int32";
          return {};
        }
        type_id_t operator()(std::string_view name) const override {
          if (name == "my_app.int32")
            return type_id_v<int32_t>;
          return invalid_type_id;
        }
      };
      alias_mapper mapper;
      auto ys = make_type_id_list<int32_t>();
      byte_buffer buf;
      binary_serializer sink{buf, policy::use_type_names};
      sink.mapper(&mapper);
      check(sink.value(ys));
      THEN("a deserializer with the same mapper roundtrips") {
        binary_deserializer source{buf, policy::use_type_names};
        source.mapper(&mapper);
        type_id_list zs = make_type_id_list();
        check(source.value(zs));
        check_eq(ys, zs);
      }
      AND_THEN("mismatched mappers fail to roundtrip") {
        byte_buffer std_buf;
        binary_serializer std_sink{std_buf, policy::use_type_names};
        check(std_sink.value(ys));
        binary_deserializer source{std_buf, policy::use_type_names};
        source.mapper(&mapper);
        type_id_list zs = make_type_id_list();
        check(!source.value(zs));
      }
    }
    WHEN("serializing with a JSON writer") {
      json_writer sink;
      check(sink.value(xs));
      THEN("the JSON contains type name strings") {
        auto json = sink.str();
        check(json.find("int32_t") != std::string::npos);
        check(json.find("string") != std::string::npos);
        check(json.find("double") != std::string::npos);
      }
      AND_THEN("a JSON reader reproduces the list") {
        json_reader source;
        type_id_list ys = make_type_id_list();
        check(source.load(sink.str()));
        check(source.value(ys));
        check_eq(xs, ys);
      }
      AND_THEN("apply roundtrips via the JSON reader") {
        json_reader source;
        type_id_list ys = make_type_id_list();
        check(source.load(sink.str()));
        check(source.apply(ys));
        check_eq(xs, ys);
      }
    }
  }
  GIVEN("an empty type ID list") {
    auto xs = make_type_id_list();
    WHEN("serializing with a binary serializer") {
      byte_buffer buf;
      binary_serializer sink{buf};
      check(sink.value(xs));
      THEN("a binary deserializer reproduces the empty list") {
        binary_deserializer source{buf};
        type_id_list ys = make_type_id_list();
        check(source.value(ys));
        check_eq(xs, ys);
        check(ys.empty());
      }
    }
    WHEN("serializing with a JSON writer") {
      json_writer sink;
      check(sink.value(xs));
      THEN("a JSON reader reproduces the empty list") {
        json_reader source;
        type_id_list ys = make_type_id_list();
        check(source.load(sink.str()));
        check(source.value(ys));
        check_eq(xs, ys);
        check(ys.empty());
      }
    }
  }
}

SCENARIO("message load rejects oversized type lists") {
  GIVEN("a binary payload exceeding the maximum size") {
    byte_buffer buf;
    binary_serializer sink{buf};
    require(sink.begin_object(type_id_v<message>, "message"));
    require(sink.begin_field("types"));
    require(sink.begin_sequence(type_id_list::max_size + 1));
    WHEN("loading the message from the binary payload") {
      binary_deserializer source{buf};
      message loaded;
      THEN("deserialization fails before reading values") {
        check(!loaded.load(source.as_deserializer()));
        check_eq(source.get_error(), sec::invalid_argument);
      }
    }
  }
}
