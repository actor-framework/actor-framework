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

#ifndef CAF_TERMINAL_STREAM_SCATTERER_HPP
#define CAF_TERMINAL_STREAM_SCATTERER_HPP

#include "caf/invalid_stream_scatterer.hpp"

namespace caf {

/// Special-purpose scatterer for sinks that terminate a stream. A terminal
/// stream scatterer generates infinite credit.
class terminal_stream_scatterer : public invalid_stream_scatterer {
public:
  using super = invalid_stream_scatterer;

  terminal_stream_scatterer(local_actor* self);

  ~terminal_stream_scatterer() override;

  size_t capacity() const noexcept override;
};

} // namespace caf

#endif // CAF_TERMINAL_STREAM_SCATTERER_HPP
