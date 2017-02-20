/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_STREAM_HPP
#define CAF_STREAM_HPP

#include "caf/stream_id.hpp"
#include "caf/meta/type_name.hpp"

namespace caf {

struct invalid_stream_t {};

/// Identifies an unbound sequence of messages.
template <class T>
class stream {
public:
  stream() = default;
  stream(stream&&) = default;
  stream(const stream&) = default;
  stream& operator=(stream&&) = default;
  stream& operator=(const stream&) = default;

  stream(stream_id sid) : id_(std::move(sid)) {
    // nop
  }

  stream(invalid_stream_t) {
    // nop
  }

  inline const stream_id& id() const {
    return id_;
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, stream& x) {
    return f(meta::type_name("stream"), x.id_);
  }

private:
  stream_id id_;
};

constexpr invalid_stream_t invalid_stream = invalid_stream_t{};

} // namespace caf

#endif // CAF_STREAM_HPP
