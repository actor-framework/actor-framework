// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <memory>

namespace caf::net::ssl::password {

/// Purpose of PEM password.
enum class purpose {
  /// SSL asks the password is reading/decryption.
  reading,

  /// SSL asks the password is for writing/encryption.
  writing
};

class CAF_NET_EXPORT callback {
public:
  virtual ~callback();

  /// Reads the password into the buffer @p buf of size @p len.
  /// @returns Written characters on success, -1 otherwise.
  virtual int operator()(char* buf, int len, purpose flag) = 0;
};

using callback_ptr = std::unique_ptr<callback>;

template <class PasswordCallback>
callback_ptr make_callback(PasswordCallback callback) {
  struct impl : callback {
    explicit impl(PasswordCallback&& cb) : cb_(std::move(cb)) {
      // nop
    }
    int operator()(char* buf, int len, purpose flag) override {
      return cb_(buf, len, flag);
    }
    PasswordCallback cb_;
  };
  return std::make_unique<impl>(std::move(callback));
}

} // namespace caf::net::ssl::password
