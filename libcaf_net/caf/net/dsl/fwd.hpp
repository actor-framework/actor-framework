// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::net::dsl {

enum class client_config_type;

template <class Base>
class client_config;

template <class Base>
class lazy_client_config;

template <class Base>
class socket_client_config;

template <class Base>
class conn_client_config;

template <class Base>
class fail_client_config;

} // namespace caf::net::dsl
