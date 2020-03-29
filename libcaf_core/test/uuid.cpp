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

#define CAF_SUITE uuid

#include "caf/uuid.hpp"

#include "caf/test/dsl.hpp"

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
    result.bytes().at(index) = static_cast<byte>(std::stoi(buf, nullptr, 16));
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
  auto zero = [](byte x) { return to_integer<int>(x) == 0; };
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

CAF_TEST_FIXTURE_SCOPE_END()
