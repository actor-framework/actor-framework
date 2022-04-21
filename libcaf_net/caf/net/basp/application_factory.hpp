// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/net/basp/application.hpp"
#include "caf/proxy_registry.hpp"

namespace caf::net::basp {

/// Factory for basp::application.
/// @relates doorman
class CAF_NET_EXPORT application_factory {
public:
  using application_type = basp::application;

  application_factory(proxy_registry& proxies) : proxies_(proxies) {
    // nop
  }

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  application_type make() const {
    return application_type{proxies_};
  }

private:
  proxy_registry& proxies_;
};

} // namespace caf::net::basp
