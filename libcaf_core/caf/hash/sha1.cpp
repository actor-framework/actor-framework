// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/hash/sha1.hpp"

#include <cstring>

#define SHA1CircularShift(bits, word)                                          \
  (((word) << (bits)) | ((word) >> (32 - (bits))))

namespace caf::hash {

sha1::sha1() noexcept {
  intermediate_[0] = 0x67452301;
  intermediate_[1] = 0xEFCDAB89;
  intermediate_[2] = 0x98BADCFE;
  intermediate_[3] = 0x10325476;
  intermediate_[4] = 0xC3D2E1F0;
  memset(message_block_.data(), 0, message_block_.size());
}

sha1::result_type sha1::result() noexcept {
  if (!sealed_) {
    pad_message();
    memset(message_block_.data(), 0, message_block_.size());
    length_ = 0;
    sealed_ = true;
  }
  std::array<std::byte, sha1::hash_size> buf;
  for (size_t i = 0; i < hash_size; ++i) {
    auto tmp = intermediate_[i >> 2] >> 8 * (3 - (i & 0x03));
    buf[i] = static_cast<std::byte>(tmp);
  }
  return buf;
}

bool sha1::append(const uint8_t* begin, const uint8_t* end) noexcept {
  if (sealed_) {
    emplace_error(sec::runtime_error,
                  "cannot append to a sealed SHA-1 context");
    return false;
  }
  for (auto i = begin; i != end; ++i) {
    if (length_ >= std::numeric_limits<uint64_t>::max() - 8) {
      emplace_error(sec::runtime_error, "SHA-1 message too long");
      return false;
    }
    message_block_[message_block_index_++] = *i;
    length_ += 8;
    if (message_block_index_ == 64)
      process_message_block();
  }
  return true;
}

void sha1::process_message_block() {
  const uint32_t K[] = {
    0x5A827999,
    0x6ED9EBA1,
    0x8F1BBCDC,
    0xCA62C1D6,
  };
  uint32_t W[80];         // Word sequence.
  uint32_t A, B, C, D, E; // Word buffers.
  for (auto t = 0; t < 16; t++) {
    W[t] = message_block_[t * 4] << 24;
    W[t] |= message_block_[t * 4 + 1] << 16;
    W[t] |= message_block_[t * 4 + 2] << 8;
    W[t] |= message_block_[t * 4 + 3];
  }

  for (auto t = 16; t < 80; t++) {
    W[t] = SHA1CircularShift(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
  }
  A = intermediate_[0];
  B = intermediate_[1];
  C = intermediate_[2];
  D = intermediate_[3];
  E = intermediate_[4];
  for (auto t = 0; t < 20; t++) {
    auto tmp = SHA1CircularShift(5, A) + ((B & C) | ((~B) & D)) + E + W[t]
               + K[0];
    E = D;
    D = C;
    C = SHA1CircularShift(30, B);
    B = A;
    A = tmp;
  }
  for (auto t = 20; t < 40; t++) {
    auto tmp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
    E = D;
    D = C;
    C = SHA1CircularShift(30, B);
    B = A;
    A = tmp;
  }
  for (auto t = 40; t < 60; t++) {
    auto tmp = SHA1CircularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E
               + W[t] + K[2];
    E = D;
    D = C;
    C = SHA1CircularShift(30, B);
    B = A;
    A = tmp;
  }
  for (auto t = 60; t < 80; t++) {
    auto tmp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
    E = D;
    D = C;
    C = SHA1CircularShift(30, B);
    B = A;
    A = tmp;
  }
  intermediate_[0] += A;
  intermediate_[1] += B;
  intermediate_[2] += C;
  intermediate_[3] += D;
  intermediate_[4] += E;
  message_block_index_ = 0;
}

void sha1::pad_message() {
  if (message_block_index_ > 55) {
    message_block_[message_block_index_++] = 0x80;
    while (message_block_index_ < 64)
      message_block_[message_block_index_++] = 0;
    process_message_block();
    while (message_block_index_ < 56)
      message_block_[message_block_index_++] = 0;
  } else {
    message_block_[message_block_index_++] = 0x80;
    while (message_block_index_ < 56)
      message_block_[message_block_index_++] = 0;
  }
  message_block_[56] = static_cast<uint8_t>(length_ >> 56);
  message_block_[57] = static_cast<uint8_t>(length_ >> 48);
  message_block_[58] = static_cast<uint8_t>(length_ >> 40);
  message_block_[59] = static_cast<uint8_t>(length_ >> 32);
  message_block_[60] = static_cast<uint8_t>(length_ >> 24);
  message_block_[61] = static_cast<uint8_t>(length_ >> 16);
  message_block_[62] = static_cast<uint8_t>(length_ >> 8);
  message_block_[63] = static_cast<uint8_t>(length_);
  process_message_block();
}

} // namespace caf::hash
