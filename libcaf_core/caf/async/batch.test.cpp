// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/batch.hpp"

#include "caf/test/scenario.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"

#include <list>
#include <string>
#include <vector>

using namespace caf;
using namespace std::literals;

namespace {

template <class T>
auto to_vec(span<const T> xs) {
  return std::vector<T>{xs.begin(), xs.end()};
}

template <class T>
auto to_list(span<const T> xs) {
  return std::list<T>{xs.begin(), xs.end()};
}

SCENARIO("batches are constructible from a list of values") {
  GIVEN("an empty vector") {
    WHEN("passing it to make_batch") {
      THEN("the resulting batch is empty") {
        auto uut = async::make_batch(std::vector<int>{});
        check(uut.empty());
      }
    }
  }
  GIVEN("an vector of integers") {
    WHEN("passing it to make_batch") {
      THEN("the resulting batch contains copies of the integers") {
        auto xs = std::vector{1, 2, 3, 4, 5};
        auto uut = async::make_batch(xs);
        check_eq(uut.size(), xs.size());
        require_eq(uut.item_type(), type_id_v<int>);
        check_eq(to_vec(uut.items<int>()), xs);
      }
    }
  }
  GIVEN("an vector of strings") {
    WHEN("passing it to make_batch") {
      THEN("the resulting batch contains copies of the strings") {
        auto xs = std::vector{"foo"s, "bar"s, "baz"s};
        auto uut = async::make_batch(xs);
        check_eq(uut.size(), xs.size());
        require_eq(uut.item_type(), type_id_v<std::string>);
        check_eq(to_vec(uut.items<std::string>()), xs);
      }
    }
  }
  GIVEN("an linked list of integers") {
    WHEN("passing it to make_batch") {
      THEN("the resulting batch contains copies of the integers") {
        auto xs = std::list<int>{1, 2, 3, 4, 5};
        auto uut = async::make_batch(xs);
        check_eq(uut.size(), xs.size());
        require_eq(uut.item_type(), type_id_v<int>);
        check_eq(to_list(uut.items<int>()), xs);
      }
    }
  }
  GIVEN("an linked list of strings") {
    WHEN("passing it to make_batch") {
      THEN("the resulting batch contains copies of the strings") {
        auto xs = std::list<std::string>{"foo"s, "bar"s, "baz"s};
        auto uut = async::make_batch(xs);
        check_eq(uut.size(), xs.size());
        require_eq(uut.item_type(), type_id_v<std::string>);
        check_eq(to_list(uut.items<std::string>()), xs);
      }
    }
  }
}

SCENARIO("batches are deserializable from JSON") {
  GIVEN("an empty JSON-encoded batch with type") {
    WHEN("deserializing it") {
      THEN("the resulting batch contains the same values") {
        std::string_view json = R"__({
          "type": "int32_t",
          "items": []
        })__";
        json_reader reader;
        require(reader.load(json));
        async::batch uut;
        check(inspect(reader, uut));
        check(uut.empty());
      }
    }
  }
  GIVEN("an empty JSON-encoded batch with type that omits the items field") {
    WHEN("deserializing it") {
      THEN("the resulting batch contains the same values") {
        std::string_view json = R"__({
          "type": "int32_t"
        })__";
        json_reader reader;
        require(reader.load(json));
        async::batch uut;
        check(inspect(reader, uut));
        check(uut.empty());
      }
    }
  }
  GIVEN("an empty JSON-encoded batch with type that omits both fields") {
    WHEN("deserializing it") {
      THEN("the resulting batch contains the same values") {
        std::string_view json = "{}";
        json_reader reader;
        require(reader.load(json));
        async::batch uut;
        check(inspect(reader, uut));
        check(uut.empty());
      }
    }
  }
  GIVEN("a non-empty JSON-encoded integer batch") {
    WHEN("deserializing it") {
      THEN("the resulting batch contains the same values") {
        auto xs = std::vector{1, 2, 3, 4, 5};
        std::string_view json = R"__({
          "type": "int32_t",
          "items": [1, 2, 3, 4, 5]
        })__";
        json_reader reader;
        require(reader.load(json));
        async::batch uut;
        check(inspect(reader, uut));
        check_eq(uut.size(), xs.size());
        require_eq(uut.item_type(), type_id_v<int>);
        check_eq(to_vec(uut.items<int>()), xs);
      }
    }
  }
  GIVEN("a non-empty JSON-encoded string batch") {
    WHEN("deserializing it") {
      THEN("the resulting batch contains the same values") {
        auto xs = std::vector{"foo"s, "bar"s, "baz"s};
        std::string_view json = R"__({
          "type": "std::string",
          "items": ["foo", "bar", "baz"]
        })__";
        json_reader reader;
        require(reader.load(json));
        async::batch uut;
        check(inspect(reader, uut));
        check_eq(uut.size(), xs.size());
        require_eq(uut.item_type(), type_id_v<std::string>);
        check_eq(to_vec(uut.items<std::string>()), xs);
      }
    }
  }
  GIVEN("a non-empty JSON-encoded batch with missing type field") {
    WHEN("deserializing it") {
      THEN("the inspection fails") {
        std::string_view json = R"__({
          "items": [1, 2, 3, 4, 5]
        })__";
        json_reader reader;
        require(reader.load(json));
        async::batch uut;
        check(!inspect(reader, uut));
      }
    }
  }
}

SCENARIO("batches are serializable to JSON") {
  json_writer writer;
  writer.skip_object_type_annotation(true);
  writer.indentation(0);
  GIVEN("an empty batch") {
    WHEN("serializing it") {
      THEN("the resulting JSON is an empty object") {
        async::batch uut;
        check(inspect(writer, uut));
        check_eq(writer.str(), "{}");
      }
    }
  }
  GIVEN("an non-empty integer batch") {
    WHEN("serializing it") {
      THEN("the resulting JSON contains the same values") {
        auto uut = async::make_batch(std::vector{1, 2, 3});
        check(inspect(writer, uut));
        check_eq(writer.str(),
                 R"__({"type": "int32_t", "items": [1, 2, 3]})__");
      }
    }
  }
  GIVEN("an non-empty string batch") {
    WHEN("serializing it") {
      THEN("the resulting JSON contains the same values") {
        auto uut = async::make_batch(std::vector{"foo"s, "bar"s});
        check(inspect(writer, uut));
        check_eq(writer.str(),
                 R"__({"type": "std::string", "items": ["foo", "bar"]})__");
      }
    }
  }
}

SCENARIO("serializing and then deserializing a batch makes a deep copy") {
  GIVEN("an empty batch") {
    WHEN("serializing it and deserializing a new batch from the data") {
      THEN("the new batch is empty") {
        async::batch uut;
        caf::byte_buffer buf;
        caf::binary_serializer sink{buf};
        check(inspect(sink, uut));
        caf::binary_deserializer source{buf};
        async::batch result;
        check(inspect(source, result));
        check(result.empty());
      }
    }
  }
  GIVEN("an non-empty integer batch") {
    WHEN("serializing it and deserializing a new batch from the data") {
      THEN("the new batch is empty") {
        auto xs = std::vector{1, 2, 3};
        auto uut = async::make_batch(xs);
        caf::byte_buffer buf;
        caf::binary_serializer sink{buf};
        check(inspect(sink, uut));
        caf::binary_deserializer source{buf};
        async::batch result;
        check(inspect(source, result));
        check_eq(to_vec(result.items<int>()), xs);
      }
    }
  }
  GIVEN("an non-empty string batch") {
    WHEN("serializing it and deserializing a new batch from the data") {
      THEN("the new batch is empty") {
        auto xs = std::vector{"foo"s, "bar"s, "baz"s};
        auto uut = async::make_batch(xs);
        caf::byte_buffer buf;
        caf::binary_serializer sink{buf};
        check(inspect(sink, uut));
        caf::binary_deserializer source{buf};
        async::batch result;
        check(inspect(source, result));
        check_eq(to_vec(result.items<std::string>()), xs);
      }
    }
  }
}

} // namespace
