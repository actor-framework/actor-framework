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
//#include <boost/thread/tss.hpp>

#include <pthread.h>

#include "cppa/self.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/thread_mapped_actor.hpp"

namespace cppa {

namespace {

pthread_key_t s_key;
pthread_once_t s_key_once = PTHREAD_ONCE_INIT;

local_actor* tss_constructor() {
    local_actor* result = new thread_mapped_actor;
    result->ref();
    get_scheduler()->register_converted_context(result);
    return result;
}

void tss_destructor(void* vptr) {
    if (vptr) self_type::cleanup_fun(reinterpret_cast<local_actor*>(vptr));
}

void tss_make_key() {
    pthread_key_create(&s_key, tss_destructor);
}

local_actor* tss_get() {
    pthread_once(&s_key_once, tss_make_key);
    return reinterpret_cast<local_actor*>(pthread_getspecific(s_key));
}

local_actor* tss_release() {
    pthread_once(&s_key_once, tss_make_key);
    auto result = reinterpret_cast<local_actor*>(pthread_getspecific(s_key));
    if (result) {
        pthread_setspecific(s_key, nullptr);
    }
    return result;
}

local_actor* tss_get_or_create() {
    pthread_once(&s_key_once, tss_make_key);
    auto result = reinterpret_cast<local_actor*>(pthread_getspecific(s_key));
    if (!result) {
        result = tss_constructor();
        pthread_setspecific(s_key, reinterpret_cast<void*>(result));
    }
    return result;
}

void tss_reset(local_actor* ptr, bool inc_ref_count = true) {
    pthread_once(&s_key_once, tss_make_key);
    auto old_ptr = reinterpret_cast<local_actor*>(pthread_getspecific(s_key));
    if (old_ptr) {
        tss_destructor(old_ptr);
    }
    if (ptr != nullptr && inc_ref_count) {
        ptr->ref();
    }
    pthread_setspecific(s_key, reinterpret_cast<void*>(ptr));
}

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
    tss_reset(ptr);
}

self_type::pointer self_type::get_impl() {
    return tss_get_or_create();
}

actor* self_type::convert_impl() {
    return get_impl();
}

self_type::pointer self_type::get_unchecked_impl() {
    return tss_get();
}

void self_type::adopt_impl(self_type::pointer ptr) {
    tss_reset(ptr, false);
}

self_type::pointer self_type::release_impl() {
    return tss_release();
}

} // namespace cppa
