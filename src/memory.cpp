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
 * Copyright (C) 2011-2014                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#include <vector>
#include <typeinfo>

#include "cppa/detail/memory.hpp"
#include "cppa/mailbox_element.hpp"

using namespace std;

#ifdef CPPA_DISABLE_MEM_MANAGEMENT

int cppa_memory_keep_compiler_happy() { return 0; }

#else // CPPA_DISABLE_MEM_MANAGEMENT

namespace cppa {
namespace detail {

namespace {

pthread_key_t s_key;
pthread_once_t s_key_once = PTHREAD_ONCE_INIT;

} // namespace <anonymous>

memory_cache::~memory_cache() { }

typedef map<const type_info*, unique_ptr<memory_cache> > cache_map;

void cache_map_destructor(void* ptr) {
    if (ptr) delete reinterpret_cast<cache_map*>(ptr);
}

void make_cache_map() {
    pthread_key_create(&s_key, cache_map_destructor);
}

cache_map& get_cache_map() {
    pthread_once(&s_key_once, make_cache_map);
    auto cache = reinterpret_cast<cache_map*>(pthread_getspecific(s_key));
    if (!cache) {
        cache = new cache_map;
        pthread_setspecific(s_key, cache);
        // insert default types
        unique_ptr<memory_cache> tmp(new basic_memory_cache<mailbox_element>);
        cache->insert(make_pair(&typeid(mailbox_element), move(tmp)));
    }
    return *cache;
}

memory_cache* memory::get_cache_map_entry(const type_info* tinf) {
    auto& cache = get_cache_map();
    auto i = cache.find(tinf);
    if (i != cache.end()) return i->second.get();
    return nullptr;
}

void memory::add_cache_map_entry(const type_info* tinf, memory_cache* instance) {
    auto& cache = get_cache_map();
    cache[tinf].reset(instance);
}

instance_wrapper::~instance_wrapper() { }

} // namespace util
} // namespace cppa


#endif // CPPA_DISABLE_MEM_MANAGEMENT
