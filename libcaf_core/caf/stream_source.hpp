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

#ifndef CAF_DETAIL_STREAM_SOURCE_HPP
#define CAF_DETAIL_STREAM_SOURCE_HPP

#include <tuple>

#include "caf/intrusive_ptr.hpp"
#include "caf/stream_manager.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

template <class Out, class Scatterer, class... Ts>
class stream_source : public virtual stream_manager {
public:
  // -- member types -----------------------------------------------------------

  using output_type = Out;

  // -- constructors, destructors, and assignment operators --------------------

  stream_source(scheduled_actor* self) : stream_manager(self), out_(self) {
    // nop
  }

  Scatterer& out() override {
    return out_;
  }

protected:
  Scatterer out_;
};

template <class Out, class Scatterer, class... HandshakeData>
using stream_source_ptr =
  intrusive_ptr<stream_source<Out, Scatterer, HandshakeData...>>;

template <class Scatterer, class... Ts>
using stream_source_ptr_t =
  stream_source_ptr<typename Scatterer::value_type, Scatterer,
                    detail::decay_t<Ts>...>;

} // namespace caf

#endif // CAF_DETAIL_STREAM_SOURCE_HPP
