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

#define CAF_SUITE serializer_impl

#include <vector>

#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/test/unit_test.hpp"
#include "serialization_fixture.hpp"

using namespace caf;

CAF_TEST_FIXTURE_SCOPE(deserialization_tests, serialization_fixture)

CAF_TEST("serialize to std::vector<char>") {
  using container_type = std::vector<char>;
  std::vector<char> binary_serializer_buffer;
  container_type serializer_impl_buffer;
  binary_serializer binarySerializer{sys, binary_serializer_buffer};
  serializer_impl<container_type> serializerImpl{sys, serializer_impl_buffer};
  if (auto err = binarySerializer(source))
    CAF_FAIL(
      "serialisation failed with binary_serializer: " << sys.render(err));
  if (auto err = serializerImpl(source))
    CAF_FAIL("serialisation failed with serializer_impl: " << sys.render(err));
  CAF_CHECK_EQUAL(memcmp(binary_serializer_buffer.data(),
                         serializer_impl_buffer.data(),
                         binary_serializer_buffer.size()),
                  0);
}

CAF_TEST("serialize to std::vector<byte>") {
  using container_type = std::vector<byte>;
  std::vector<char> binary_serializer_buffer;
  container_type serializer_impl_buffer;
  binary_serializer binarySerializer{sys, binary_serializer_buffer};
  serializer_impl<container_type> serializerImpl{sys, serializer_impl_buffer};
  if (auto err = binarySerializer(source))
    CAF_FAIL(
      "serialisation failed with binary_serializer: " << sys.render(err));
  if (auto err = serializerImpl(source))
    CAF_FAIL("serialisation failed with serializer_impl: " << sys.render(err));
  CAF_CHECK_EQUAL(memcmp(binary_serializer_buffer.data(),
                         serializer_impl_buffer.data(),
                         binary_serializer_buffer.size()),
                  0);
}

CAF_TEST("serialize to std::vector<uint8_t>") {
  using container_type = std::vector<uint8_t>;
  std::vector<char> binary_serializer_buffer;
  container_type serializer_impl_buffer;
  binary_serializer binarySerializer{sys, binary_serializer_buffer};
  serializer_impl<container_type> serializerImpl{sys, serializer_impl_buffer};
  if (auto err = binarySerializer(source))
    CAF_FAIL(
      "serialisation failed with binary_serializer: " << sys.render(err));
  if (auto err = serializerImpl(source))
    CAF_FAIL("serialisation failed with serializer_impl: " << sys.render(err));
  CAF_CHECK_EQUAL(memcmp(binary_serializer_buffer.data(),
                         serializer_impl_buffer.data(),
                         binary_serializer_buffer.size()),
                  0);
}

CAF_TEST_FIXTURE_SCOPE_END();