// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE uuid

#include "caf/uuid.hpp"

#include "core-test.hpp"

#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"

using namespace caf;

namespace {

uuid operator"" _uuid(const char* cstr, size_t cstr_len) {
  if (cstr_len != 36)
    CAF_FAIL("malformed test input");
  std::string str{cstr, cstr_len};
  if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
    CAF_FAIL("malformed test input");
  str.erase(std::remove(str.begin(), str.end(), '-'), str.end());
  if (str.size() != 32)
    CAF_FAIL("malformed test input");
  uuid result;
  for (size_t index = 0; index < 16; ++index) {
    auto input_index = index * 2;
    char buf[] = {str[input_index], str[input_index + 1], '\0'};
    result.bytes().at(index)
      = static_cast<std::byte>(std::stoi(buf, nullptr, 16));
  }
  return result;
}

struct fixture {
  uuid nil; // 00000000-0000-0000-0000-000000000000

  // A couple of UUIDs for version 1.
  uuid v1[3] = {
    "cbba341a-6ceb-11ea-bc55-0242ac130003"_uuid,
    "cbba369a-6ceb-11ea-bc55-0242ac130003"_uuid,
    "cbba38fc-6ceb-11ea-bc55-0242ac130003"_uuid,
  };

  // A couple of UUIDs for version 4.
  uuid v4[3] = {
    "2ee4ded7-69c0-4dd6-876d-02e446b21784"_uuid,
    "934a33b6-7f0c-4d70-9749-5ad4292358dd"_uuid,
    "bf761f7c-00f2-4161-855e-e286cfa63c11"_uuid,
  };
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(uuid_tests, fixture)

CAF_TEST(default generated UUIDs have all 128 bits set to zero) {
  uuid nil;
  CAF_CHECK(!nil);
  auto zero = [](std::byte x) { return std::to_integer<int>(x) == 0; };
  CAF_CHECK(std::all_of(nil.bytes().begin(), nil.bytes().end(), zero));
  CAF_CHECK(nil == uuid::nil());
}

CAF_TEST(UUIDs print in 4 2 2 2 6 format) {
  CAF_CHECK_EQUAL(to_string(nil), "00000000-0000-0000-0000-000000000000");
  CAF_CHECK_EQUAL(to_string(v1[0]), "cbba341a-6ceb-11ea-bc55-0242ac130003");
  CAF_CHECK_EQUAL(to_string(v1[1]), "cbba369a-6ceb-11ea-bc55-0242ac130003");
  CAF_CHECK_EQUAL(to_string(v1[2]), "cbba38fc-6ceb-11ea-bc55-0242ac130003");
}

CAF_TEST(make_uuid parses strings in 4 2 2 2 6 format) {
  CAF_CHECK_EQUAL(make_uuid("00000000-0000-0000-0000-000000000000"), nil);
  CAF_CHECK_EQUAL(make_uuid("cbba341a-6ceb-11ea-bc55-0242ac130003"), v1[0]);
  CAF_CHECK_EQUAL(make_uuid("cbba369a-6ceb-11ea-bc55-0242ac130003"), v1[1]);
  CAF_CHECK_EQUAL(make_uuid("cbba38fc-6ceb-11ea-bc55-0242ac130003"), v1[2]);
}

CAF_TEST(make_uuid rejects strings with invalid variant or version values) {
  CAF_CHECK(!uuid::can_parse("cbba341a-6ceb-81ea-bc55-0242ac130003"));
  CAF_CHECK(!uuid::can_parse("cbba369a-6ceb-F1ea-bc55-0242ac130003"));
  CAF_CHECK(!uuid::can_parse("cbba38fc-6ceb-01ea-bc55-0242ac130003"));
  CAF_CHECK_EQUAL(make_uuid("cbba341a-6ceb-81ea-bc55-0242ac130003"),
                  pec::invalid_argument);
  CAF_CHECK_EQUAL(make_uuid("cbba369a-6ceb-F1ea-bc55-0242ac130003"),
                  pec::invalid_argument);
  CAF_CHECK_EQUAL(make_uuid("cbba38fc-6ceb-01ea-bc55-0242ac130003"),
                  pec::invalid_argument);
}

#define WITH(uuid_str) if (auto x = uuid_str##_uuid; true)

CAF_TEST(version 1 defines UUIDs that are based on time) {
  CAF_CHECK_EQUAL(v1[0].version(), uuid::time_based);
  CAF_CHECK_EQUAL(v1[1].version(), uuid::time_based);
  CAF_CHECK_EQUAL(v1[2].version(), uuid::time_based);
  CAF_CHECK_NOT_EQUAL(v4[0].version(), uuid::time_based);
  CAF_CHECK_NOT_EQUAL(v4[1].version(), uuid::time_based);
  CAF_CHECK_NOT_EQUAL(v4[2].version(), uuid::time_based);
  WITH("00000001-0000-1000-8122-334455667788") {
    CAF_CHECK_EQUAL(x.variant(), uuid::rfc4122);
    CAF_CHECK_EQUAL(x.version(), uuid::time_based);
    CAF_CHECK_EQUAL(x.timestamp(), 0x0000000000000001ull);
    CAF_CHECK_EQUAL(x.clock_sequence(), 0x0122u);
    CAF_CHECK_EQUAL(x.node(), 0x334455667788ull);
  }
  WITH("00000001-0001-1000-8122-334455667788") {
    CAF_CHECK_EQUAL(x.variant(), uuid::rfc4122);
    CAF_CHECK_EQUAL(x.version(), uuid::time_based);
    CAF_CHECK_EQUAL(x.timestamp(), 0x0000000100000001ull);
    CAF_CHECK_EQUAL(x.clock_sequence(), 0x0122u);
    CAF_CHECK_EQUAL(x.node(), 0x334455667788ull);
  }
  WITH("00000001-0001-1001-8122-334455667788") {
    CAF_CHECK_EQUAL(x.variant(), uuid::rfc4122);
    CAF_CHECK_EQUAL(x.version(), uuid::time_based);
    CAF_CHECK_EQUAL(x.timestamp(), 0x0001000100000001ull);
    CAF_CHECK_EQUAL(x.clock_sequence(), 0x0122u);
    CAF_CHECK_EQUAL(x.node(), 0x334455667788ull);
  }
  WITH("ffffffff-ffff-1fff-bfff-334455667788") {
    CAF_CHECK_EQUAL(x.variant(), uuid::rfc4122);
    CAF_CHECK_EQUAL(x.version(), uuid::time_based);
    CAF_CHECK_EQUAL(x.timestamp(), 0x0FFFFFFFFFFFFFFFull);
    CAF_CHECK_EQUAL(x.clock_sequence(), 0x3FFFu);
    CAF_CHECK_EQUAL(x.node(), 0x334455667788ull);
  }
}

SCENARIO("UUIDs are inspectable") {
  auto id = "2ee4ded7-69c0-4dd6-876d-02e446b21784"_uuid;
  GIVEN("a binary serializer") {
    byte_buffer buf;
    binary_serializer sink{nullptr, buf};
    WHEN("applying an UUID to the serializer") {
      CHECK(sink.apply(id));
      THEN("a binary deserializer reproduces the UUID") {
        binary_deserializer source{nullptr, buf};
        uuid id_copy;
        CHECK(source.apply(id_copy));
        CHECK_EQ(id, id_copy);
      }
    }
  }
  GIVEN("a JSON writer") {
    json_writer sink;
    WHEN("applying an UUID to the writer") {
      CHECK(sink.apply(id));
      THEN("the writer renders the UUID as string") {
        CHECK_EQ(sink.str(), R"("2ee4ded7-69c0-4dd6-876d-02e446b21784")");
      }
      AND("a JSON reader reproduces the UUID") {
        json_reader source;
        uuid id_copy;
        CHECK(source.load(sink.str()));
        CHECK(source.apply(id_copy));
        CHECK_EQ(id, id_copy);
      }
    }
  }
}

SCENARIO("UUIDs are hashable") {
  GIVEN("two UUIDs ") {
    auto id1 = "2ee4ded7-69c0-4dd6-876d-02e446b21784"_uuid;
    auto id2 = "a6155548-2994-4833-b4e3-9823f5f15fe9"_uuid;
    WHEN("retrieving a hash value for the UUIDs") {
      THEN("the UUIDs return different hash values") {
        std::hash<uuid> f;
        CHECK_EQ(id1.hash(), f(id1));
        CHECK_EQ(id2.hash(), f(id2));
        CHECK_NE(f(id1), f(id2));
      }
    }
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
