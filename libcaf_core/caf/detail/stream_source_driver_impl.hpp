/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_STREAM_SOURCE_DRIVER_IMPL_HPP
#define CAF_STREAM_SOURCE_DRIVER_IMPL_HPP

#include "caf/none.hpp"
#include "caf/stream_source_driver.hpp"
#include "caf/stream_source_trait.hpp"

namespace caf {
namespace detail {

template <class Output, class Pull, class Done, class HandshakeData>
class stream_source_driver_impl;

/// Identifies an unbound sequence of messages.
template <class Output, class Pull, class Done, class... Ts>
class stream_source_driver_impl<Output, Pull, Done, std::tuple<Ts...>> final
    : public stream_source_driver<Output, Ts...> {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_source_driver<Output, Ts...>;

  using output_type = typename super::output_type;

  using stream_type = stream<output_type>;

  using output_stream_type = typename super::output_stream_type;

  using tuple_type = std::tuple<Ts...>;

  using handshake_tuple_type = typename super::handshake_tuple_type;

  using trait = stream_source_trait_t<Pull>;

  using state_type = typename trait::state;

  template <class Init, class Tuple>
  stream_source_driver_impl(Init init, Pull f, Done pred, Tuple&& hs)
      : pull_(std::move(f)),
        done_(std::move(pred)),
        hs_(std::forward<Tuple>(hs)) {
    init(state_);
  }

  handshake_tuple_type make_handshake() const override {
    return std::tuple_cat(std::make_tuple(none), hs_);
  }

  void pull(downstream<output_type>& out, size_t num) override {
    return pull_(state_, out, num);
  }

  bool done() const noexcept override {
    return done_(state_);
  }

private:
  state_type state_;
  Pull pull_;
  Done done_;
  tuple_type hs_;
};

} // namespace detail
} // namespace caf

#endif // CAF_STREAM_SOURCE_DRIVER_IMPL_HPP
