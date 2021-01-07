// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <functional>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// @relates local_actor
/// Default handler function that leaves messages in the mailbox.
/// Can also be used inside custom message handlers to signalize
/// skipping to the runtime.
class CAF_CORE_EXPORT skip_t {
public:
  using fun = std::function<skippable_result(scheduled_actor* self, message&)>;

  constexpr skip_t() {
    // nop
  }

  operator fun() const;
};

/// Tells the runtime system to skip a message when used as message
/// handler, i.e., causes the runtime to leave the message in
/// the mailbox of an actor.
constexpr skip_t skip = skip_t{};

} // namespace caf
