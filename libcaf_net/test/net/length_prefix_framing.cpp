// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.length_prefix_framing

#include "caf/net/length_prefix_framing.hpp"

#include "caf/test/dsl.hpp"

#include <deque>
#include <numeric>
#include <vector>

#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/span.hpp"
#include "caf/tag/message_oriented.hpp"

using namespace caf;

namespace {

/// upper layer: expect messages
/// Needs to be initilized by the layer two steps down.
struct ul_expect_messages {
  using input_tag = tag::message_oriented;

  void set_expected_messages(std::vector<byte_buffer> messages) {
    expected_messages.clear();
    for (auto& msg : messages)
      expected_messages.emplace_back(std::move(msg));
  }

  void add_expected_messages(std::vector<byte_buffer> messages) {
    for (auto& msg : messages)
      expected_messages.emplace_back(std::move(msg));
  }

  template <class LowerLayer>
  ptrdiff_t consume(LowerLayer&, byte_span buffer) {
    CAF_REQUIRE(expected_messages.size() > 0);
    auto& next = expected_messages.front();
    CAF_CHECK_EQUAL(next.size(), buffer.size());
    CAF_CHECK(std::equal(next.begin(), next.end(), buffer.begin()));
    expected_messages.pop_front();
    return buffer.size();
  }

  std::deque<byte_buffer> expected_messages;
};

/// lower layer: offer stream for message parsing
template <class UpperLayer>
struct ll_provide_stream_for_messages {
  using output_tag = tag::stream_oriented;

  void set_expectations(std::vector<byte> data,
                        std::vector<byte_buffer> messages) {
    data_stream = std::move(data);
    auto& checking_layer = upper_layer.upper_layer();
    checking_layer.set_expected_messages(messages);
  }

  void add_expectations(const std::vector<byte>& data,
                        std::vector<byte_buffer> messages) {
    data_stream.insert(data_stream.end(), data.begin(), data.end());
    auto& checking_layer = upper_layer.upper_layer();
    checking_layer.add_expect_messages(messages);
  }

  void run() {
    CAF_CHECK(data_stream.size() != 0);
    while (processed != data_stream.size()) {
      auto all_data = make_span(data_stream.data() + processed,
                                data_stream.size() - processed);
      auto new_data = make_span(data_stream.data() + offered,
                                data_stream.size() - offered);
      auto newly_offered = new_data.size();
      auto consumed = upper_layer.consume(*this, all_data, new_data);
      CAF_CHECK(consumed >= 0);
      CAF_CHECK(static_cast<size_t>(consumed) <= data_stream.size());
      offered += newly_offered;
      processed += consumed;
      if (consumed > 0) {
        data_stream.erase(data_stream.begin(), data_stream.begin() + consumed);
        offered -= processed;
        processed = 0;
      }
      if (consumed == 0 || data_stream.size() == 0)
        return;
    }
  }

  size_t processed = 0;
  size_t offered = 0;

  std::vector<byte> data_stream;

  UpperLayer upper_layer;
};

template <class... Ts>
byte_buffer to_buf(const Ts&... xs) {
  byte_buffer buf;
  binary_serializer sink{nullptr, buf};
  if (!sink.apply_objects(xs...))
    CAF_FAIL("to_buf failed: " << sink.get_error());
  return buf;
}

void encode_message(std::vector<byte>& data, const byte_buffer& message) {
  using detail::to_network_order;
  auto current_size = data.size();
  data.insert(data.end(), 4, byte{0});
  auto msg_begin = data.begin() + current_size;
  auto msg_size = message.size();
  auto u32_size = to_network_order(static_cast<uint32_t>(msg_size));
  memcpy(std::addressof(*msg_begin), &u32_size, 4);
  data.insert(data.end(), message.begin(), message.end());
}

struct fixture {
  using test_layers = ll_provide_stream_for_messages<
    net::length_prefix_framing<ul_expect_messages>>;

  void generate_messages(size_t num, size_t factor = 10) {
    for (size_t n = 1; n <= num; n += 1) {
      std::vector<int> buf(n * factor);
      std::iota(buf.begin(), buf.end(), n);
      messages.emplace_back(to_buf(buf));
    }
    for (auto& msg : messages)
      encode_message(data, msg);
  }

  void set_expectations() {
    layers.set_expectations(data, messages);
  }

  void test_receive_data() {
    layers.run();
  }

  void clear() {
    data.clear();
    messages.clear();
  }

  test_layers layers;

  std::vector<byte> data;
  std::vector<byte_buffer> messages;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(length_prefix_framing_tests, fixture)

CAF_TEST(process messages) {
  // Single message.
  generate_messages(1);
  set_expectations();
  test_receive_data();
  clear();
  // Multiple messages.
  generate_messages(10);
  set_expectations();
  test_receive_data();
}

CAF_TEST(incomplete message) {
  generate_messages(1, 1000);
  CAF_MESSAGE("data.size() = " << data.size());
  auto initial_size = data.size();
  auto data_copy = data;
  auto mid = data.size() / 2;
  data.resize(mid);
  CAF_MESSAGE("data.size() = " << data.size());
  data_copy.erase(data_copy.begin(), data_copy.begin() + mid);
  CAF_MESSAGE("data_copy.size() = " << data_copy.size());
  CAF_REQUIRE(data.size() + data_copy.size() == initial_size);
  // Don't set expectations because there shouldn't be a complete message
  // in the bytes.
  auto messages_copy = messages;
  messages.clear();
  CAF_REQUIRE(messages.empty());
  set_expectations();
  test_receive_data();
  data.insert(data.end(), data_copy.begin(), data_copy.end());
  messages = messages_copy;
  set_expectations();
  test_receive_data();
}

CAF_TEST_FIXTURE_SCOPE_END()
