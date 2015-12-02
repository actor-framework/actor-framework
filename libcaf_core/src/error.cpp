/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/error.hpp"

#include "caf/config.hpp"
#include "caf/serializer.hpp"
#include "caf/deserializer.hpp"

namespace caf {

error::error() : code_(0), category_(atom("")) {
  // nop
}

error::error(uint8_t x, atom_value y, std::string z)
    : code_(x),
      category_(y),
      context_(std::move(z)) {
  // nop
}

uint8_t error::code() const {
  return code_;
}

atom_value error::category() const {
  return category_;
}

std::string& error::context() {
  return context_;
}

const std::string& error::context() const {
  return context_;
}

error::operator bool() const {
  return code_ != 0;
}

void error::clear() {
  code_ = 0;
}

uint32_t error::compress_code_and_size() const {
  auto res = static_cast<uint32_t>(context_.size());
  res <<= 8;
  res |= static_cast<uint32_t>(code_);
  return res;
}

void serialize(serializer& sink, error& x, const unsigned int) {
  // truncat oversized strings
  if (x.context_.size() > 0xFFFFFF)
    x.context_.resize(0xFFFFFF);
  auto flag_size_and_code = x.compress_code_and_size();
  if (x.category_ == atom("") || x.code_ == 0) {
    flag_size_and_code |= 0x80000000;
    sink << flag_size_and_code;
    return;
  }
  sink << flag_size_and_code;
  sink << x.category_;
  if (! x.context_.empty())
    sink.apply_raw(x.context_.size(), const_cast<char*>(x.context_.data()));
}

void serialize(deserializer& source, error& x, const unsigned int) {
  x.context_.clear();
  uint32_t flag_size_and_code;
  source >> flag_size_and_code;
  if (flag_size_and_code & 0x80000000) {
    x.category_ = atom("");
    x.code_ = 0;
    return;
  }
  source >> x.category_;
  x.code_ = static_cast<uint8_t>(flag_size_and_code & 0xFF);
  auto size = flag_size_and_code >> 8;
  if (size > 0) {
    x.context_.resize(size);
    source.apply_raw(size, &x.context_[0]);
  }
}

int error::compare(uint8_t x, atom_value y) const {
  if (category_ < y)
    return -1;
  if (category_ > y)
    return 1;
  return static_cast<int>(code_) - x;
}

int error::compare(const error& x) const {
  return compare(x.code(), x.category());
}

std::string to_string(const error& x) {
  std::string result = "<error: (";
  result += to_string(x.category());
  result += ", ";
  result += std::to_string(static_cast<int>(x.code()));
  result += ")>";
  return result;
}

} // namespace caf
