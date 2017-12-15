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

#ifndef CAF_STREAM_SINK_DRIVER_IMPL_HPP
#define CAF_STREAM_SINK_DRIVER_IMPL_HPP

#include "caf/none.hpp"
#include "caf/stream_sink_driver.hpp"
#include "caf/stream_sink_trait.hpp"

namespace caf {
namespace detail {

/// Identifies an unbound sequence of messages.
template <class Input, class Process, class Finalize>
class stream_sink_driver_impl final : public stream_sink_driver<Input> {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_sink_driver<Input>;

  using input_type = typename super::input_type;

  using trait = stream_sink_trait_t<Process, Finalize>;

  using output_type = typename trait::output;

  using state_type = typename trait::state;

  template <class Init>
  stream_sink_driver_impl(Init init, Process f, Finalize fin)
      : process_(std::move(f)),
        finalize_(std::move(fin)) {
    init(state_);
  }

  void process(std::vector<input_type>&& xs) override {
    return trait::process::invoke(process_, state_, std::move(xs));
  }

  message finalize() override {
    return trait::finalize::invoke(finalize_, state_);
  }

private:
  Process process_;
  Finalize finalize_;
  state_type state_;
};

} // namespace detail
} // namespace caf

#endif // CAF_STREAM_SINK_DRIVER_IMPL_HPP
