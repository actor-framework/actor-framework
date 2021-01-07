// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/stream_slot.hpp"

namespace caf {

class CAF_CORE_EXPORT stream_aborter : public attachable {
public:
  enum mode { source_aborter, sink_aborter };

  struct token {
    const actor_addr& observer;
    stream_slot slot;
    mode m;
    static constexpr size_t token_type = attachable::token::stream_aborter;
  };

  stream_aborter(actor_addr&& observed, actor_addr&& observer, stream_slot slot,
                 mode m);

  ~stream_aborter() override;

  void actor_exited(const error& rsn, execution_unit* host) override;

  bool matches(const attachable::token& what) override;

  /// Adds a stream aborter to `observed`.
  static void
  add(strong_actor_ptr observed, actor_addr observer, stream_slot slot, mode m);

  /// Removes a stream aborter from `observed`.
  static void del(strong_actor_ptr observed, const actor_addr& observer,
                  stream_slot slot, mode m);

private:
  actor_addr observed_;
  actor_addr observer_;
  stream_slot slot_;
  mode mode_;
};

CAF_CORE_EXPORT attachable_ptr make_stream_aborter(actor_addr observed,
                                                   actor_addr observer,
                                                   stream_slot slot,
                                                   stream_aborter::mode m);

} // namespace caf
