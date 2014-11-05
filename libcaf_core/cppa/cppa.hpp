/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CPPA_CPPA_HPP
#define CPPA_CPPA_HPP

// <backward_compatibility version="0.9" whole_file="yes">

// include new header
#include "caf/all.hpp"

// include compatibility headers
#include "cppa/opt.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/publish_local_groups.hpp"

// headers for classes and functions that have been moved
#include "caf/io/all.hpp"

// be nice to code using legacy macros
#define CPPA_VERSION CAF_VERSION

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
