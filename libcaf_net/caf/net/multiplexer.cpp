// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/multiplexer.hpp"

#include "caf/net/default_multiplexer.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/action.hpp"
#include "caf/actor_system.hpp"
#include "caf/byte.hpp"
#include "caf/config.hpp"
#include "caf/detail/pollset_updater.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

#include <algorithm>
#include <optional>

#ifndef CAF_WINDOWS
#  include <poll.h>
#  include <signal.h>
#else
#  include "caf/detail/socket_sys_includes.hpp"
#endif // CAF_WINDOWS

namespace caf::net {

// -- static utility functions -------------------------------------------------

#ifdef CAF_LINUX

void multiplexer::block_sigpipe() {
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  sigset_t saved_mask;
  if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
    perror("pthread_sigmask");
    exit(1);
  }
}

#else

void multiplexer::block_sigpipe() {
  // nop
}

#endif

multiplexer_ptr multiplexer::make_default(middleman* parent) {
  return default_multiplexer::make(parent);
}

multiplexer* multiplexer::from(actor_system& sys) {
  return sys.network_manager().mpx_ptr();
}

// -- constructors, destructors, and assignment operators ----------------------

multiplexer::~multiplexer() {
  // nop
}

} // namespace caf::net
