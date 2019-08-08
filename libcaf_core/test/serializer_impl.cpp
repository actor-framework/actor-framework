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

#include "caf/test/unit_test.hpp"

#include <algorithm>
#include <caf/byte.hpp>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <memory>
#include <new>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/make_type_erased_tuple_view.hpp"
#include "caf/make_type_erased_view.hpp"
#include "caf/message.hpp"
#include "caf/message_handler.hpp"
#include "caf/primitive_variant.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/ref_counted.hpp"
#include "caf/serializer.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/stream_deserializer.hpp"
#include "caf/stream_serializer.hpp"
#include "caf/streambuf.hpp"
#include "caf/variant.hpp"

#include "caf/detail/enum_to_string.hpp"
#include "caf/detail/get_mac_addresses.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/type_traits.hpp"

using namespace std;
using namespace caf;
using caf::detail::type_erased_value_impl;

namespace {

enum class test_enum {
  a,
  b,
  c,
};

struct test_data {
  int32_t i32 = -345;
  int64_t i64 = -1234567890123456789ll;
  float f32 = 3.45f;
  double f64 = 54.3;
  duration dur = duration{time_unit::seconds, 123};
  timestamp ts = timestamp{timestamp::duration{1478715821 * 1000000000ll}};
  test_enum te = test_enum::b;
  string str = "Lorem ipsum dolor sit amet.";
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, test_data& x) {
  return f(meta::type_name("test_data"), x.i32, x.i64, x.f32, x.f64, x.dur,
           x.ts, x.te, x.str);
}
} // namespace

CAF_TEST("serialize to std::vector<char>") {
  using container_type = std::vector<char>;
  actor_system_config cfg;
  actor_system sys{cfg};
  test_data data;
  std::vector<char> binary_serializer_buffer;
  container_type serializer_impl_buffer;
  binary_serializer binarySerializer{sys, binary_serializer_buffer};
  serializer_impl<container_type> serializerImpl{sys, serializer_impl_buffer};
  binarySerializer(data);
  serializerImpl(data);
  CAF_CHECK_EQUAL(memcmp(binary_serializer_buffer.data(),
                         serializer_impl_buffer.data(),
                         binary_serializer_buffer.size()),
                  0);
}

CAF_TEST("serialize to std::vector<byte>") {
  using container_type = std::vector<byte>;
  actor_system_config cfg;
  actor_system sys{cfg};
  test_data data;
  std::vector<char> binary_serializer_buffer;
  container_type serializer_impl_buffer;
  binary_serializer binarySerializer{sys, binary_serializer_buffer};
  serializer_impl<container_type> serializerImpl{sys, serializer_impl_buffer};
  binarySerializer(data);
  serializerImpl(data);
  CAF_CHECK_EQUAL(memcmp(binary_serializer_buffer.data(),
                         serializer_impl_buffer.data(),
                         binary_serializer_buffer.size()),
                  0);
}

CAF_TEST("serialize to std::vector<uint8_t>") {
  using container_type = std::vector<uint8_t>;
  actor_system_config cfg;
  actor_system sys{cfg};
  test_data data;
  std::vector<char> binary_serializer_buffer;
  container_type serializer_impl_buffer;
  binary_serializer binarySerializer{sys, binary_serializer_buffer};
  serializer_impl<container_type> serializerImpl{sys, serializer_impl_buffer};
  binarySerializer(data);
  serializerImpl(data);
  CAF_CHECK_EQUAL(memcmp(binary_serializer_buffer.data(),
                         serializer_impl_buffer.data(),
                         binary_serializer_buffer.size()),
                  0);
}
