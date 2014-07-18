/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CAF_IO_PUBLISH_IMPL_HPP
#define CAF_IO_PUBLISH_IMPL_HPP

#include "caf/abstract_actor.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

#include "caf/io/network.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"

namespace caf {
namespace io {

template<class ActorHandle, class SocketAcceptor>
void publish_impl(ActorHandle whom, SocketAcceptor fd) {
    using namespace detail;
    auto mm = middleman::instance();
    // we can't move fd into our lambda in C++11 ...
    using pair_type = std::pair<ActorHandle, SocketAcceptor>;
    auto data = std::make_shared<pair_type>(std::move(whom), std::move(fd));
    mm->run_later([mm, data] {
        auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
        bro->publish(std::move(data->first), std::move(data->second));
    });
}

inline void publish_impl(abstract_actor_ptr whom, uint16_t port,
                         const char* ipaddr) {
    using namespace detail;
    auto mm = middleman::instance();
    auto addr = whom->address();
    network::default_socket_acceptor fd{mm->backend()};
    network::ipv4_bind(fd, port, ipaddr);
    publish_impl(std::move(whom), std::move(fd));
}

} // namespace io
} // namespace caf

#endif // CAF_IO_PUBLISH_IMPL_HPP
