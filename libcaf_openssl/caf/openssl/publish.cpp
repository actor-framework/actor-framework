// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/openssl/publish.hpp"

#include "caf/openssl/manager.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/detail/assert.hpp"
#include "caf/expected.hpp"
#include "caf/log/openssl.hpp"
#include "caf/scoped_actor.hpp"

#include <set>

namespace caf::openssl {

expected<uint16_t> publish(actor_system& sys, const strong_actor_ptr& whom,
                           std::set<std::string>&& sigs, uint16_t port,
                           const char* cstr, bool ru) {
  auto lg = log::openssl::trace("whom = {}, sigs = {}, port = {}", whom, sigs,
                                port);
  CAF_ASSERT(whom != nullptr);
  std::string in;
  if (cstr != nullptr)
    in = cstr;
  auto self = scoped_actor{sys};
  return self
    ->mail(publish_atom_v, port, std::move(whom), std::move(sigs),
           std::move(in), ru)
    .request(sys.openssl_manager().actor_handle(), infinite)
    .receive();
}

} // namespace caf::openssl
