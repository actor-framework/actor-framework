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

#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
#include "caf/fwd.hpp"
#include "caf/stream_slot.hpp"

namespace caf {

class stream_aborter : public attachable {
public:
  enum mode {
    source_aborter,
    sink_aborter
  };

  struct token {
    const actor_addr& observer;
    stream_slot slot;
    mode m;
    static constexpr size_t token_type = attachable::token::stream_aborter;
  };

  stream_aborter(actor_addr&& observed, actor_addr&& observer,
                 stream_slot slot, mode m);

  ~stream_aborter() override;

  void actor_exited(const error& rsn, execution_unit* host) override;

  bool matches(const attachable::token& what) override;

  /// Adds a stream aborter to `observed`.
  static void add(strong_actor_ptr observed, actor_addr observer,
                  stream_slot slot, mode m);

  /// Removes a stream aborter from `observed`.
  static void del(strong_actor_ptr observed, const actor_addr& observer,
                  stream_slot slot, mode m);

private:
  actor_addr observed_;
  actor_addr observer_;
  stream_slot slot_;
  mode mode_;
};

attachable_ptr make_stream_aborter(actor_addr observed, actor_addr observer,
                                   stream_slot slot, stream_aborter::mode m);

} // namespace caf


