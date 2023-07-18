// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/middleman_actor.hpp"

#include "caf/detail/openssl_export.hpp"

namespace caf::openssl {

CAF_OPENSSL_EXPORT io::middleman_actor make_middleman_actor(actor_system& sys,
                                                            actor db);

} // namespace caf::openssl
