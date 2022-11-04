// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.rfc6455

#include "caf/detail/rfc6455.hpp"

#include "caf/test/dsl.hpp"

#include "caf/byte.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"

#include <cstdint>
#include <initializer_list>
#include <vector>

using namespace caf;

namespace {

struct fixture {
  using impl = detail::rfc6455;

  auto bytes(std::initializer_list<uint8_t> xs) {
    std::vector<byte> result;
    for (auto x : xs)
      result.emplace_back(static_cast<byte>(x));
    return result;
  }

  template <class T>
  auto take(const T& xs, size_t num_bytes) {
    auto n = std::min(xs.size(), num_bytes);
    return std::vector<typename T::value_type>{xs.begin(), xs.begin() + n};
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(rfc6455_tests, fixture)

CAF_TEST(masking) {
  auto key = uint32_t{0xDEADC0DE};
  auto data = bytes({0x12, 0x34, 0x45, 0x67, 0x89, 0x9A});
  auto masked_data = data;
  CAF_MESSAGE("masking XORs the repeated key to data");
  impl::mask_data(key, masked_data);
  CAF_CHECK_EQUAL(masked_data, bytes({
                                 0x12 ^ 0xDE,
                                 0x34 ^ 0xAD,
                                 0x45 ^ 0xC0,
                                 0x67 ^ 0xDE,
                                 0x89 ^ 0xDE,
                                 0x9A ^ 0xAD,
                               }));
  CAF_MESSAGE("masking masked data again gives the original data");
  impl::mask_data(key, masked_data);
  CAF_CHECK_EQUAL(masked_data, data);
}

CAF_TEST(no mask key plus small data) {
  std::vector<uint8_t> data{0x12, 0x34, 0x45, 0x67};
  std::vector<byte> out;
  impl::assemble_frame(impl::binary_frame, 0, as_bytes(make_span(data)), out);
  CAF_CHECK_EQUAL(out, bytes({
                         0x82,                   // FIN + binary frame opcode
                         0x04,                   // data size = 4
                         0x12, 0x34, 0x45, 0x67, // masked data
                       }));
  impl::header hdr;
  CAF_CHECK_EQUAL(impl::decode_header(out, hdr), 2);
  CAF_CHECK_EQUAL(hdr.fin, true);
  CAF_CHECK_EQUAL(hdr.mask_key, 0u);
  CAF_CHECK_EQUAL(hdr.opcode, impl::binary_frame);
  CAF_CHECK_EQUAL(hdr.payload_len, data.size());
}

CAF_TEST(valid mask key plus small data) {
  std::vector<uint8_t> data{0x12, 0x34, 0x45, 0x67};
  std::vector<byte> out;
  impl::assemble_frame(impl::binary_frame, 0xDEADC0DE,
                       as_bytes(make_span(data)), out);
  CAF_CHECK_EQUAL(out, bytes({
                         0x82,                   // FIN + binary frame opcode
                         0x84,                   // MASKED + data size = 4
                         0xDE, 0xAD, 0xC0, 0xDE, // mask key,
                         0x12, 0x34, 0x45, 0x67, // masked data
                       }));
  impl::header hdr;
  CAF_CHECK_EQUAL(impl::decode_header(out, hdr), 6);
  CAF_CHECK_EQUAL(hdr.fin, true);
  CAF_CHECK_EQUAL(hdr.mask_key, 0xDEADC0DEu);
  CAF_CHECK_EQUAL(hdr.opcode, impl::binary_frame);
  CAF_CHECK_EQUAL(hdr.payload_len, data.size());
}

CAF_TEST(no mask key plus medium data) {
  std::vector<uint8_t> data;
  data.insert(data.end(), 126, 0xFF);
  std::vector<byte> out;
  impl::assemble_frame(impl::binary_frame, 0, as_bytes(make_span(data)), out);
  CAF_CHECK_EQUAL(take(out, 8),
                  bytes({
                    0x82,                   // FIN + binary frame opcode
                    0x7E,                   // 126 -> uint16 size
                    0x00, 0x7E,             // data size = 126
                    0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
                  }));
  impl::header hdr;
  CAF_CHECK_EQUAL(impl::decode_header(out, hdr), 4);
  CAF_CHECK_EQUAL(hdr.fin, true);
  CAF_CHECK_EQUAL(hdr.mask_key, 0u);
  CAF_CHECK_EQUAL(hdr.opcode, impl::binary_frame);
  CAF_CHECK_EQUAL(hdr.payload_len, data.size());
}

CAF_TEST(valid mask key plus medium data) {
  std::vector<uint8_t> data;
  data.insert(data.end(), 126, 0xFF);
  std::vector<byte> out;
  impl::assemble_frame(impl::binary_frame, 0xDEADC0DE,
                       as_bytes(make_span(data)), out);
  CAF_CHECK_EQUAL(take(out, 12),
                  bytes({
                    0x82,                   // FIN + binary frame opcode
                    0xFE,                   // MASKED + 126 -> uint16 size
                    0x00, 0x7E,             // data size = 126
                    0xDE, 0xAD, 0xC0, 0xDE, // mask key,
                    0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
                  }));
  impl::header hdr;
  CAF_CHECK_EQUAL(impl::decode_header(out, hdr), 8);
  CAF_CHECK_EQUAL(hdr.fin, true);
  CAF_CHECK_EQUAL(hdr.mask_key, 0xDEADC0DEu);
  CAF_CHECK_EQUAL(hdr.opcode, impl::binary_frame);
  CAF_CHECK_EQUAL(hdr.payload_len, data.size());
}

CAF_TEST(no mask key plus large data) {
  std::vector<uint8_t> data;
  data.insert(data.end(), 65536, 0xFF);
  std::vector<byte> out;
  impl::assemble_frame(impl::binary_frame, 0, as_bytes(make_span(data)), out);
  CAF_CHECK_EQUAL(take(out, 14),
                  bytes({
                    0x82, // FIN + binary frame opcode
                    0x7F, // 127 -> uint64 size
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, // 65536
                    0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
                  }));
  impl::header hdr;
  CAF_CHECK_EQUAL(impl::decode_header(out, hdr), 10);
  CAF_CHECK_EQUAL(hdr.fin, true);
  CAF_CHECK_EQUAL(hdr.mask_key, 0u);
  CAF_CHECK_EQUAL(hdr.opcode, impl::binary_frame);
  CAF_CHECK_EQUAL(hdr.payload_len, data.size());
}

CAF_TEST(valid mask key plus large data) {
  std::vector<uint8_t> data;
  data.insert(data.end(), 65536, 0xFF);
  std::vector<byte> out;
  impl::assemble_frame(impl::binary_frame, 0xDEADC0DE,
                       as_bytes(make_span(data)), out);
  CAF_CHECK_EQUAL(take(out, 18),
                  bytes({
                    0x82, // FIN + binary frame opcode
                    0xFF, // MASKED + 127 -> uint64 size
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, // 65536
                    0xDE, 0xAD, 0xC0, 0xDE,                         // mask key,
                    0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
                  }));
  impl::header hdr;
  CAF_CHECK_EQUAL(impl::decode_header(out, hdr), 14);
  CAF_CHECK_EQUAL(hdr.fin, true);
  CAF_CHECK_EQUAL(hdr.mask_key, 0xDEADC0DEu);
  CAF_CHECK_EQUAL(hdr.opcode, impl::binary_frame);
  CAF_CHECK_EQUAL(hdr.payload_len, data.size());
}

CAF_TEST_FIXTURE_SCOPE_END()
