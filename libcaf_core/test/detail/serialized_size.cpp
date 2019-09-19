/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE detail.serialized_size

#include "caf/detail/serialized_size.hpp"

#include "caf/test/dsl.hpp"

#include <vector>

#include "caf/byte.hpp"
#include "caf/serializer_impl.hpp"

using namespace caf;

using caf::detail::serialized_size;

namespace {

struct fixture : test_coordinator_fixture<> {
  using buffer_type = std::vector<byte>;

  template <class... Ts>
  size_t actual_size(const Ts&... xs) {
    buffer_type buf;
    serializer_impl<buffer_type> sink{sys, buf};
    if (auto err = sink(xs...))
      CAF_FAIL("failed to serialize data: " << sys.render(err));
    return buf.size();
  }
};

} // namespace

#define CHECK_SAME_SIZE(...)                                                   \
  CAF_CHECK_EQUAL(serialized_size(sys, __VA_ARGS__), actual_size(__VA_ARGS__))

CAF_TEST_FIXTURE_SCOPE(serialized_size_tests, fixture)

CAF_TEST(numbers) {
  CHECK_SAME_SIZE(int8_t{42});
  CHECK_SAME_SIZE(int16_t{42});
  CHECK_SAME_SIZE(int32_t{42});
  CHECK_SAME_SIZE(int64_t{42});
  CHECK_SAME_SIZE(uint8_t{42});
  CHECK_SAME_SIZE(uint16_t{42});
  CHECK_SAME_SIZE(uint32_t{42});
  CHECK_SAME_SIZE(uint64_t{42});
  CHECK_SAME_SIZE(4.2f);
  CHECK_SAME_SIZE(4.2);
}

CAF_TEST(containers) {
  CHECK_SAME_SIZE(std::string{"foobar"});
  CHECK_SAME_SIZE(std::vector<char>({'a', 'b', 'c'}));
  CHECK_SAME_SIZE(std::vector<std::string>({"hello", "world"}));
}

CAF_TEST(messages) {
  CHECK_SAME_SIZE(make_message(42));
  CHECK_SAME_SIZE(make_message(1, 2, 3));
  CHECK_SAME_SIZE(make_message("hello", "world"));
}

CAF_TEST_FIXTURE_SCOPE_END()
