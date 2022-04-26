// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/ssl/tls.hpp"

#include "caf/config.hpp"

CAF_PUSH_WARNINGS
#include <openssl/ssl.h>
CAF_POP_WARNINGS

namespace caf::net::ssl {

int native(tls x) {
  switch (x) {
    default:
      return 0;
    case tls::v1_0:
      return TLS1_VERSION;
    case tls::v1_1:
      return TLS1_1_VERSION;
    case tls::v1_2:
      return TLS1_2_VERSION;
    case tls::v1_3:
      return TLS1_3_VERSION;
  }
}

} // namespace caf::net::ssl
