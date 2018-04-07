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

// The rationale of this header is to provide a serialization API
// that is compatbile to boost.serialization. In particular, the
// design goals are:
// - allow users to integrate existing boost.serialization-based code easily
// - allow to switch out this header with the actual boost header in boost.actor
//
// Differences in semantics are:
// - CAF does *not* respect class versions
// - the `unsigned int` argument is always 0 and ignored by CAF
//
// Since CAF requires all runtime instances to have the same types
// announced, different class versions in a single actor system would
// cause inconsistencies that are not recoverable.

#pragma once

#include <utility>
#include <type_traits>

#include "caf/detail/type_traits.hpp"

namespace boost {
namespace serialization {

} // namespace serialization
} // namespace boost

namespace caf {
namespace detail {

// Calls `serialize(...)` with `using namespace boost::serialization`
// to enable both ADL and picking up existing boost code.

template <class Processor, class U>
auto delegate_serialize(Processor& proc, U& x, const unsigned int y = 0)
  -> decltype(serialize(proc, x, y)) {
  using namespace boost::serialization;
  serialize(proc, x, y);
}

// Calls `serialize(...)` without the unused version argument, which CAF
// ignores anyway.

template <class Processor, class U>
auto delegate_serialize(Processor& proc, U& x)
  -> decltype(serialize(proc, x)) {
  serialize(proc, x);
}

} // namespace detail
} // namespace caf

