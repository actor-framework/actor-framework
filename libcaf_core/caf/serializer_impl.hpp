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

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/serializer.hpp"

namespace caf {

/// Implements the serializer interface with a binary serialization protocol.
template <class Container>
class serializer_impl final : public serializer {
public:
  // -- member types -----------------------------------------------------------

  using super = serializer;

  using container_type = Container;

  using value_type = typename container_type::value_type;

  // -- constructors, destructors, and assignment operators --------------------

  serializer_impl(actor_system& sys, container_type& buf)
    : super(sys), buf_(buf), write_pos_(buf_.size()) {
    // nop
  }

  serializer_impl(execution_unit* ctx, container_type& buf)
    : super(ctx), buf_(buf), write_pos_(buf_.size()) {
    // nop
  }

  // -- position management ----------------------------------------------------

  /// Sets the write position to given offset.
  /// @pre `offset <= buf.size()`
  void seek(size_t offset) {
    write_pos_ = offset;
  }

  /// Jumps `num_bytes` forward. Resizes the buffer (filling it with zeros)
  /// when skipping past the end.
  void skip(size_t num_bytes) {
    auto remaining = buf_.size() - write_pos_;
    if (remaining < num_bytes)
      buf_.insert(buf_.end(), num_bytes - remaining, 0);
    write_pos_ += num_bytes;
  }

  // -- overridden member functions --------------------------------------------

  error begin_object(uint16_t& nr, std::string& name) override {
    if (nr != 0)
      return apply(nr);
    if (auto err = apply(nr))
      return err;
    return apply(name);
  }

  error end_object() override {
    return none;
  }

  error begin_sequence(size_t& list_size) override {
    // Use varbyte encoding to compress sequence size on the wire.
    // For 64-bit values, the encoded representation cannot get larger than 10
    // bytes. A scratch space of 16 bytes suffices as upper bound.
    uint8_t buf[16];
    auto i = buf;
    auto x = static_cast<uint32_t>(list_size);
    while (x > 0x7f) {
      *i++ = (static_cast<uint8_t>(x) & 0x7f) | 0x80;
      x >>= 7;
    }
    *i++ = static_cast<uint8_t>(x) & 0x7f;
    apply_raw(static_cast<size_t>(i - buf), buf);
    return none;
  }

  error end_sequence() override {
    return none;
  }

  error apply_raw(size_t num_bytes, void* data) override {
    CAF_ASSERT(write_pos_ <= buf_.size());
    static_assert((sizeof(value_type) == 1), "sizeof(value_type) > 1");
    auto ptr = reinterpret_cast<value_type*>(data);
    auto buf_size = buf_.size();
    if (write_pos_ == buf_size) {
      buf_.insert(buf_.end(), ptr, ptr + num_bytes);
    } else if (write_pos_ + num_bytes <= buf_size) {
      memcpy(buf_.data() + write_pos_, ptr, num_bytes);
    } else {
      auto remaining = buf_size - write_pos_;
      CAF_ASSERT(remaining < num_bytes);
      memcpy(buf_.data() + write_pos_, ptr, remaining);
      buf_.insert(buf_.end(), ptr + remaining, ptr + num_bytes);
    }
    write_pos_ += num_bytes;
    CAF_ASSERT(write_pos_ <= buf_.size());
    return none;
  }

  // -- properties -------------------------------------------------------------

  container_type& buf() {
    return buf_;
  }

  const container_type& buf() const {
    return buf_;
  }

  size_t write_pos() const noexcept {
    return write_pos_;
  }

protected:
  error apply_impl(int8_t& x) override {
    return apply_raw(sizeof(int8_t), &x);
  }

  error apply_impl(uint8_t& x) override {
    return apply_raw(sizeof(uint8_t), &x);
  }

  error apply_impl(int16_t& x) override {
    return apply_int(*this, static_cast<uint16_t>(x));
  }

  error apply_impl(uint16_t& x) override {
    return apply_int(*this, x);
  }

  error apply_impl(int32_t& x) override {
    return apply_int(*this, static_cast<uint32_t>(x));
  }

  error apply_impl(uint32_t& x) override {
    return apply_int(*this, x);
  }

  error apply_impl(int64_t& x) override {
    return apply_int(*this, static_cast<uint64_t>(x));
  }

  error apply_impl(uint64_t& x) override {
    return apply_int(*this, x);
  }

  error apply_impl(float& x) override {
    return apply_int(*this, detail::pack754(x));
  }

  error apply_impl(double& x) override {
    return apply_int(*this, detail::pack754(x));
  }

  error apply_impl(long double& x) override {
    // The IEEE-754 conversion does not work for long double
    // => fall back to string serialization (event though it sucks).
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<long double>::digits) << x;
    auto tmp = oss.str();
    return apply_impl(tmp);
  }

  error apply_impl(std::string& x) override {
    auto str_size = x.size();
    if (str_size == 0)
      return error::eval([&] { return begin_sequence(str_size); },
                         [&] { return end_sequence(); });
    auto data = const_cast<char*>(x.c_str());
    return error::eval([&] { return begin_sequence(str_size); },
                       [&] { return apply_raw(str_size, data); },
                       [&] { return end_sequence(); });
  }

  error apply_impl(std::u16string& x) override {
    auto str_size = x.size();
    if (auto err = begin_sequence(str_size))
      return err;
    for (auto c : x) {
      // The standard does not guarantee that char16_t is exactly 16 bits.
      if (auto err = apply_int(*this, static_cast<uint16_t>(c)))
        return err;
    }
    return none;
  }

  error apply_impl(std::u32string& x) override {
    auto str_size = x.size();
    if (auto err = begin_sequence(str_size))
      return err;
    for (auto c : x) {
      // The standard does not guarantee that char32_t is exactly 32 bits.
      if (auto err = apply_int(*this, static_cast<uint32_t>(c)))
        return err;
    }
    return none;
  }

private:
  template <class T>
  error apply_int(serializer_impl<Container>& bs, T x) {
    auto y = detail::to_network_order(x);
    return bs.apply_raw(sizeof(T), &y);
  }

  container_type& buf_;
  size_t write_pos_;
};

} // namespace caf
