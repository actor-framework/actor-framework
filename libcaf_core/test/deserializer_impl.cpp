/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE deserializer_impl

#include "caf/test/unit_test.hpp"

#include <vector>

#include "serialization_fixture.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/deserializer_impl.hpp"
#include "caf/byte.hpp"

using namespace caf;

CAF_TEST_FIXTURE_SCOPE(deserialization_tests, serialization_fixture)

CAF_TEST("deserialize std::vector<char>") {
  using container_type = std::vector<char>;
  container_type buffer;
  serializer_impl<container_type> serializer{sys, buffer};
  if (auto err = serializer(source))
    CAF_FAIL("serialisation with serializer_impl<std::vector<char>> failed: "
             << sys.render(err));
  deserializer_impl<container_type> deserializer{sys, buffer};
  if (auto err = deserializer(sink))
    CAF_FAIL("deserialisation with deserializer_impl<std::vector<char>> failed:"
             << sys.render(err));
  CAF_CHECK_EQUAL(source, sink);
}

CAF_TEST("deserialize std::vector<byte>") {
  using container_type = std::vector<byte>;
  container_type buffer;
  serializer_impl<container_type> serializer{sys, buffer};
  if (auto err = serializer(source))
    CAF_FAIL("serialisation with serializer_impl<std::vector<byte>> failed: "
             << sys.render(err));
  deserializer_impl<container_type> deserializer{sys, buffer};
  if (auto err = deserializer(sink))
    CAF_FAIL("deserialisation with deserializer_impl<std::vector<byte>> failed:"
             << sys.render(err));
  CAF_CHECK_EQUAL(source, sink);
}

CAF_TEST("deserialize std::vector<uint8_t>") {
  using container_type = std::vector<uint8_t>;
  container_type buffer;
  serializer_impl<container_type> serializer{sys, buffer};
  if (auto err = serializer(source))
    CAF_FAIL("serialisation with serializer_impl<std::vector<uint8_t>> failed: "
             << sys.render(err));
  deserializer_impl<container_type> deserializer{sys, buffer};
  if (auto err = deserializer(sink))
    CAF_FAIL(
      "deserialisation with deserializer_impl<std::vector<uint8_t>> failed:"
      << sys.render(err));
  CAF_CHECK_EQUAL(source, sink);
}

CAF_TEST_FIXTURE_SCOPE_END()
