// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/ssl/dtls.hpp"

#include "caf/config.hpp"

CAF_PUSH_WARNINGS
#include <openssl/ssl.h>
CAF_POP_WARNINGS

namespace caf::net::ssl {

int native(dtls x) {
  switch (x) {
    default:
      return 0;
    case dtls::v1_0:
      return DTLS1_VERSION;
    case dtls::v1_2:
      return DTLS1_2_VERSION;
  }
}

} // namespace caf::net::ssl
