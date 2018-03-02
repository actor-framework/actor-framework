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

#ifndef CAF_STREAM_STAGE_DRIVER_IMPL_HPP
#define CAF_STREAM_STAGE_DRIVER_IMPL_HPP

#include "caf/stream_slot.hpp"
#include "caf/stream_stage_driver.hpp"
#include "caf/stream_stage_trait.hpp"

namespace caf {
namespace detail {

template <class Input, class Scatterer, class Process, class Finalize,
          class HandshakeData>
class stream_stage_driver_impl;

/// Default implementation for a `stream_stage_driver` that hardwires `message`
/// as result type and implements `process` and `finalize` using user-provided
/// function objects (usually lambdas).
template <class Input, class Scatterer, class Process, class Finalize,
          class... Ts>
class stream_stage_driver_impl<Input, Scatterer, Process, Finalize,
                               std::tuple<Ts...>>
  final : public stream_stage_driver<Input, message, Scatterer, Ts...> {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_stage_driver<Input, message, Scatterer, Ts...>;

  using typename super::input_type;

  using typename super::output_type;

  using typename super::stream_type;

  using typename super::handshake_tuple_type;

  using trait = stream_stage_trait_t<Process>;

  using state_type = typename trait::state;

  template <class Init, class Tuple>
  stream_stage_driver_impl(Init init, Process f, Finalize fin, Tuple&& hs)
      : process_(std::move(f)),
        fin_(std::move(fin)),
        hs_(std::forward<Tuple>(hs)) {
    init(state_);
  }

  handshake_tuple_type make_handshake(stream_slot slot) const override {
    return std::tuple_cat(std::make_tuple(stream_type{slot}), hs_);
  }

  void process(std::vector<input_type>&& batch,
               downstream<output_type>& out) override {
    trait::process::invoke(process_, state_, std::move(batch), out);
  }

  void add_result(message& x) override {
    // The default driver assumes to receive only a single result.
    result_ = std::move(x);
  }

  message make_final_result() override {
    return std::move(result_);
  }

  void finalize(const error&) override {
    return fin_(state_);
  }

private:
  state_type state_;
  Process process_;
  Finalize fin_;
  std::tuple<Ts...> hs_;
  message result_;
};

} // namespace detail
} // namespace caf

#endif // CAF_STREAM_STAGE_DRIVER_IMPL_HPP
