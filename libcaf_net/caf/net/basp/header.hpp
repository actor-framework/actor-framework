/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <cstdint>

#include "caf/detail/comparable.hpp"
#include "caf/meta/hex_formatted.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/net/basp/message_type.hpp"

namespace caf {
namespace net {
namespace basp {

/// @addtogroup BASP

struct header : detail::comparable<header> {
  message_type type;
  uint32_t payload_len;
  uint64_t operation_data;

  constexpr header() noexcept
    : type(message_type::client_handshake), payload_len(0), operation_data(0) {
    // nop
  }

  constexpr header(message_type type, uint32_t payload_len,
                   uint64_t operation_data) noexcept
    : type(type), payload_len(payload_len), operation_data(operation_data) {
    // nop
  }

  header(const header&) noexcept = default;

  header& operator=(const header&) noexcept = default;

  int compare(const header& other) const noexcept;
};

/// Size of a BASP header in serialized form
constexpr size_t header_size = 13;

/// @relates header
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, header& x) {
  return f(meta::type_name("basp::header"), x.type, x.payload_len,
           x.operation_data);
}

/// @}

} // namespace basp
} // namespace net
} // namespace caf
