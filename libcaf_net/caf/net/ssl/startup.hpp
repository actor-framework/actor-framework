// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

namespace caf::net::ssl {

/// Initializes the SSL layer. Depending on the version, this may be mandatory
/// to call before accessing any SSL functions (OpenSSL prior to version 1.1) or
/// it may have no effect (newer versions of OpenSSL).
CAF_NET_EXPORT void startup();

/// Cleans up any state for the SSL layer. Like @ref startup, this step is
/// mandatory for some versions of the linked SSL library.
CAF_NET_EXPORT void cleanup();

} // namespace caf::net::ssl
