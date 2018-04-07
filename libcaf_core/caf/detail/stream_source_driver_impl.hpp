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

#pragma once

#include "caf/none.hpp"
#include "caf/stream_finalize_trait.hpp"
#include "caf/stream_source_driver.hpp"
#include "caf/stream_source_trait.hpp"

namespace caf {
namespace detail {

/// Identifies an unbound sequence of messages.
template <class DownstreamManager, class Pull, class Done, class Finalize>
class stream_source_driver_impl final
    : public stream_source_driver<DownstreamManager> {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_source_driver<DownstreamManager>;

  using output_type = typename super::output_type;

  using trait = stream_source_trait_t<Pull>;

  using state_type = typename trait::state;

  template <class Init>
  stream_source_driver_impl(Init init, Pull f, Done pred, Finalize fin)
      : pull_(std::move(f)),
        done_(std::move(pred)),
        fin_(std::move(fin)) {
    init(state_);
  }

  void pull(downstream<output_type>& out, size_t num) override {
    return pull_(state_, out, num);
  }

  bool done() const noexcept override {
    return done_(state_);
  }

  void finalize(const error& err) override {
    stream_finalize_trait<Finalize, state_type>::invoke(fin_, state_, err);
  }

private:
  state_type state_;
  Pull pull_;
  Done done_;
  Finalize fin_;
};

} // namespace detail
} // namespace caf

