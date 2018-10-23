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

#include <string>
#include <iterator>
#include <typeinfo>


#include "caf/config.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_cow_ptr.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"
#include "caf/type_erased_tuple.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

class message_data : public ref_counted, public type_erased_tuple {
public:
  // -- nested types -----------------------------------------------------------

  using cow_ptr = intrusive_cow_ptr<message_data>;

  // -- constructors, destructors, and assignment operators --------------------

  message_data() = default;
  message_data(const message_data&) = default;

  ~message_data() override;

  // -- pure virtual observers -------------------------------------------------

  virtual cow_ptr copy() const = 0;

  // -- observers --------------------------------------------------------------

  using type_erased_tuple::copy;

  bool shared() const noexcept override;
};

} // namespace detail
} // namespace caf

