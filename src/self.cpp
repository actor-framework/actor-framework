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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


// for thread_specific_ptr
// needed unless the new keyword "thread_local" works in GCC
#include <boost/thread/tss.hpp>

#include "cppa/self.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/thread_mapped_actor.hpp"

namespace cppa {

namespace {

boost::thread_specific_ptr<local_actor> t_this_actor(&self_type::cleanup_fun);

} // namespace <anonymous>

void self_type::cleanup_fun(cppa::local_actor* what) {
    if (what) {
        auto ptr = dynamic_cast<thread_mapped_actor*>(what);
        if (ptr) {
            // make sure "unspawned" actors quit properly
            ptr->cleanup(cppa::exit_reason::normal);
        }
        if (what->deref() == false) delete what;
    }
}

void self_type::set_impl(self_type::pointer ptr) {
    if (ptr) ptr->ref();
    t_this_actor.reset(ptr);
}

self_type::pointer self_type::get_impl() {
    auto result = t_this_actor.get();
    if (result == nullptr) {
        result = new thread_mapped_actor;
        result->ref();
        get_scheduler()->register_converted_context(result);
        t_this_actor.reset(result);
    }
    return result;
}

actor* self_type::convert_impl() {
    return get_impl();
}

self_type::pointer self_type::get_unchecked_impl() {
    return t_this_actor.get();
}

void self_type::adopt_impl(self_type::pointer ptr) {
    t_this_actor.reset(ptr);
}

self_type::pointer self_type::release_impl() {
    return t_this_actor.release();
}

} // namespace cppa
