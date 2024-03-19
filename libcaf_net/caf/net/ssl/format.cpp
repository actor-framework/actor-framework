// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/ssl/format.hpp"

#include "caf/config.hpp"
#include "caf/detail/assert.hpp"

CAF_PUSH_WARNINGS
#include <openssl/ssl.h>
CAF_POP_WARNINGS

namespace caf::net::ssl {

int native(format x) {
  switch (x) {
    default:
      CAF_ASSERT(x == format::pem);
      return SSL_FILETYPE_PEM;
    case format::asn1:
      return SSL_FILETYPE_ASN1;
  }
}

} // namespace caf::net::ssl
