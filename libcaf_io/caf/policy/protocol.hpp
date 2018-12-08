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

#include "caf/config.hpp"

#include "caf/callback.hpp"
#include "caf/error.hpp"

namespace caf {
namespace io {

template <class T>
struct newb;

} // namespace io

namespace policy {

using byte_buffer = std::vector<char>;
using header_writer = caf::callback<byte_buffer&>;

struct protocol_base {
  virtual ~protocol_base();

  virtual error read(char* bytes, size_t count) = 0;

  virtual error timeout(atom_value, uint32_t) = 0;

  virtual void write_header(byte_buffer&, header_writer*) = 0;

  virtual void prepare_for_sending(byte_buffer&, size_t, size_t, size_t) = 0;
};

template <class T>
struct protocol : protocol_base {
  using message_type = T;
  virtual ~protocol() override {
    // nop
  }

  virtual void init(io::newb<message_type>* parent) = 0;
};

template <class T>
using protocol_ptr = std::unique_ptr<protocol<T>>;

template <class T>
struct generic_protocol
    : public protocol<typename T::message_type> {

  void init(io::newb<typename T::message_type>* parent) override {
    impl.init(parent);
  }

  error read(char* bytes, size_t count) override {
    return impl.read(bytes, count);
  }

  error timeout(atom_value atm, uint32_t id) override {
    return impl.timeout(atm, id);
  }

  void write_header(byte_buffer& buf, header_writer* hw) override {
    impl.write_header(buf, hw);
  }

  void prepare_for_sending(byte_buffer& buf, size_t hstart,
                           size_t offset, size_t plen) override {
    impl.prepare_for_sending(buf, hstart, offset, plen);
  }

private:
  T impl;
};

} // namespace policy
} // namespace caf
