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

#ifndef CPPA_CPPA_HPP
#define CPPA_CPPA_HPP

// <backward_compatibility version="0.9" whole_file="yes">

// include new header
#include "caf/all.hpp"

// include compatibility headers
#include "cppa/opt.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/publish_local_groups.hpp"

// headers for classes and functions that have been moved
#include "caf/io/all.hpp"

// set old namespace to new namespace
namespace cppa = caf;

// re-import a couple of moved things backinto the global CAF namespace
namespace caf {

using io::accept_handle;
using io::connection_handle;
using io::publish;
using io::remote_actor;
using io::max_msg_size;
using io::typed_publish;
using io::typed_remote_actor;
using io::publish_local_groups;
using io::new_connection_msg;
using io::new_data_msg;
using io::connection_closed_msg;
using io::acceptor_closed_msg;

} // namespace caf

// </backward_compatibility>

#endif // CPPA_CPPA_HPP
