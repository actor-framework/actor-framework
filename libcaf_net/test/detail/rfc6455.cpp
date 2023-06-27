// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.rfc6455

#include "caf/detail/rfc6455.hpp"

#include "net-test.hpp"

#include "caf/byte_buffer.hpp"
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
    byte_buffer result;
    for (auto x : xs)
      result.emplace_back(static_cast<std::byte>(x));
    return result;
  }

  template <class T>
  auto take(const T& xs, size_t num_bytes) {
    auto n = std::min(xs.size(), num_bytes);
    return std::vector<typename T::value_type>{xs.begin(), xs.begin() + n};
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

TEST_CASE("masking") {
  auto key = uint32_t{0xDEADC0DE};
  auto data = bytes({0x12, 0x34, 0x45, 0x67, 0x89, 0x9A});
  auto masked_data = data;
  MESSAGE("masking XORs the repeated key to data");
  impl::mask_data(key, masked_data);
  CHECK_EQ(masked_data, bytes({
                          0x12 ^ 0xDE,
                          0x34 ^ 0xAD,
                          0x45 ^ 0xC0,
                          0x67 ^ 0xDE,
                          0x89 ^ 0xDE,
                          0x9A ^ 0xAD,
                        }));
  MESSAGE("masking masked data again gives the original data");
  impl::mask_data(key, masked_data);
  CHECK_EQ(masked_data, data);
}

TEST_CASE("no mask key and no data") {
  std::vector<uint8_t> data;
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0, as_bytes(make_span(data)), out);
  CHECK_EQ(out, bytes({
                  0x82, // FIN + binary frame opcode
                  0x00, // data size = 0
                }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 2);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0u);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("valid mask key and no data") {
  std::vector<uint8_t> data;
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0xDEADC0DE,
                       as_bytes(make_span(data)), out);
  CHECK_EQ(out, bytes({
                  0x82,                   // FIN + binary frame opcode
                  0x80,                   // MASKED + data size = 0
                  0xDE, 0xAD, 0xC0, 0xDE, // mask key,
                }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 6);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0xDEADC0DE);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("no mask key plus small data") {
  std::vector<uint8_t> data{0x12, 0x34, 0x45, 0x67};
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0, as_bytes(make_span(data)), out);
  CHECK_EQ(out, bytes({
                  0x82,                   // FIN + binary frame opcode
                  0x04,                   // data size = 4
                  0x12, 0x34, 0x45, 0x67, // masked data
                }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 2);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0u);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("valid mask key plus small data") {
  std::vector<uint8_t> data{0x12, 0x34, 0x45, 0x67};
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0xDEADC0DE,
                       as_bytes(make_span(data)), out);
  CHECK_EQ(out, bytes({
                  0x82,                   // FIN + binary frame opcode
                  0x84,                   // MASKED + data size = 4
                  0xDE, 0xAD, 0xC0, 0xDE, // mask key,
                  0x12, 0x34, 0x45, 0x67, // masked data
                }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 6);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0xDEADC0DE);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("no mask key plus upper bound on small data") {
  std::vector<uint8_t> data;
  data.resize(125, 0xFF);
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0, as_bytes(make_span(data)), out);
  CHECK_EQ(take(out, 6), bytes({
                           0x82,                   // FIN + binary frame opcode
                           0x7D,                   // data size = 125
                           0xFF, 0xFF, 0xFF, 0xFF, // masked data
                         }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 2);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0u);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("valid mask key plus upper bound on small data") {
  std::vector<uint8_t> data;
  data.resize(125, 0xFF);
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0xDEADC0DE,
                       as_bytes(make_span(data)), out);
  CHECK_EQ(take(out, 10), bytes({
                            0x82,                   // FIN + binary frame opcode
                            0xFD,                   // MASKED + data size = 125
                            0xDE, 0xAD, 0xC0, 0xDE, // mask key,
                            0xFF, 0xFF, 0xFF, 0xFF, // masked data
                          }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 6);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0xDEADC0DE);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("no mask key plus medium data") {
  std::vector<uint8_t> data;
  data.resize(126, 0xFF);
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0, as_bytes(make_span(data)), out);
  CHECK_EQ(take(out, 8),
           bytes({
             0x82,                   // FIN + binary frame opcode
             0x7E,                   // 126 -> uint16 size
             0x00, 0x7E,             // data size = 126
             0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
           }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 4);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0u);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("valid mask key plus medium data") {
  std::vector<uint8_t> data;
  data.resize(126, 0xFF);
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0xDEADC0DE,
                       as_bytes(make_span(data)), out);
  CHECK_EQ(take(out, 12),
           bytes({
             0x82,                   // FIN + binary frame opcode
             0xFE,                   // MASKED + 126 -> uint16 size
             0x00, 0x7E,             // data size = 126
             0xDE, 0xAD, 0xC0, 0xDE, // mask key,
             0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
           }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 8);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0xDEADC0DE);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("no mask key plus upper bound on medium data") {
  std::vector<uint8_t> data;
  data.resize(65535, 0xFF);
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0, as_bytes(make_span(data)), out);
  CHECK_EQ(take(out, 8),
           bytes({
             0x82,                   // FIN + binary frame opcode
             0x7E,                   // 126 -> uint16 size
             0xFF, 0xFF,             // data size = 65535
             0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
           }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 4);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0u);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("valid mask key plus upper bound on medium data") {
  std::vector<uint8_t> data;
  data.resize(65535, 0xFF);
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0xDEADC0DE,
                       as_bytes(make_span(data)), out);
  CHECK_EQ(take(out, 12),
           bytes({
             0x82,                   // FIN + binary frame opcode
             0xFE,                   // 126 -> uint16 size
             0xFF, 0xFF,             // data size = 65535
             0xDE, 0xAD, 0xC0, 0xDE, // mask key,
             0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
           }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 8);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0xDEADC0DE);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("no mask key plus large data") {
  std::vector<uint8_t> data;
  data.resize(65536, 0xFF);
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0, as_bytes(make_span(data)), out);
  CHECK_EQ(take(out, 14),
           bytes({
             0x82, // FIN + binary frame opcode
             0x7F, // 127 -> uint64 size
             0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, // 65536
             0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
           }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 10);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0u);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

TEST_CASE("valid mask key plus large data") {
  std::vector<uint8_t> data;
  data.resize(65536, 0xFF);
  byte_buffer out;
  impl::assemble_frame(impl::binary_frame, 0xDEADC0DE,
                       as_bytes(make_span(data)), out);
  CHECK_EQ(take(out, 18),
           bytes({
             0x82, // FIN + binary frame opcode
             0xFF, // MASKED + 127 -> uint64 size
             0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, // 65536
             0xDE, 0xAD, 0xC0, 0xDE,                         // mask key,
             0xFF, 0xFF, 0xFF, 0xFF, // first 4 masked bytes
           }));
  impl::header hdr;
  CHECK_EQ(impl::decode_header(out, hdr), 14);
  CHECK_EQ(hdr.fin, true);
  CHECK_EQ(hdr.mask_key, 0xDEADC0DE);
  CHECK_EQ(hdr.opcode, impl::binary_frame);
  CHECK_EQ(hdr.payload_len, data.size());
}

END_FIXTURE_SCOPE()
