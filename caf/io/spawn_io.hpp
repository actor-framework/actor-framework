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

#ifndef CAF_IO_SPAWN_IO_HPP
#define CAF_IO_SPAWN_IO_HPP

#include "caf/spawn.hpp"
#include "caf/connection_handle.hpp"

#include "caf/io/broker.hpp"

namespace caf {
namespace io {

/**
 * @brief Spawns a new functor-based broker.
 */
template<spawn_options Os = no_spawn_options,
         typename F = std::function<void(broker*)>, typename... Ts>
actor spawn_io(F fun, Ts&&... args) {
    return spawn<broker::functor_based>(std::move(fun),
                                        std::forward<Ts>(args)...);
}

/**
 * @brief Spawns a new functor-based broker connecting to <tt>host:port</tt>.
 */
template<spawn_options Os = no_spawn_options,
          typename F = std::function<void(broker*)>, typename... Ts>
actor spawn_io_client(F fun, const std::string& host,
                      uint16_t port, Ts&&... args) {
    network::default_socket sock{middleman::instance()->backend()};
    network::ipv4_connect(sock, host, port);
    auto hdl = network::conn_hdl_from_socket(sock);
    // provoke compiler error early
    using fun_res = decltype(fun((broker*) 0, hdl, std::forward<Ts>(args)...));
    // prevent warning about unused local type
    static_assert(std::is_same<fun_res, fun_res>::value,
                  "your compiler is lying to you");
    return spawn_class<broker::functor_based>(
                nullptr,
                [&](broker* ptr) {
                    auto hdl2 = ptr->add_connection(std::move(sock));
                    CAF_REQUIRE(hdl == hdl2);
                    static_cast<void>(hdl2); // prevent warning
                },
                std::move(fun), hdl, std::forward<Ts>(args)...);
}

struct is_socket_test {
    template<typename T>
    static auto test(T* ptr) -> decltype(ptr->native_handle(),
                                         std::true_type{});
    template<typename>
    static auto test(...) -> std::false_type;
};

template<typename T>
struct is_socket : decltype(is_socket_test::test<T>(0)) {
};

/**
 * @brief Spawns a new broker as server running on given @p port.
 */
template<spawn_options Os = no_spawn_options,
          typename F = std::function<void(broker*)>,
          typename Socket = network::default_socket_acceptor, typename... Ts>
typename std::enable_if<is_socket<Socket>::value, actor>::type
spawn_io_server(F fun, Socket sock, Ts&&... args) {
    return spawn_class<broker::functor_based>(
                nullptr,
                [&](broker* ptr) {
                    ptr->add_acceptor(std::move(sock));
                },
                std::move(fun), std::forward<Ts>(args)...);
}

/**
 * @brief Spawns a new broker as server running on given @p port.
 */
template<spawn_options Os = no_spawn_options,
          typename F = std::function<void(broker*)>, typename... Ts>
actor spawn_io_server(F fun, uint16_t port, Ts&&... args) {
    network::default_socket_acceptor fd{middleman::instance()->backend()};
    network::ipv4_bind(fd, port);
    return spawn_io_server(std::move(fun), std::move(fd),
                           std::forward<Ts>(args)...);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_SPAWN_IO_HPP
