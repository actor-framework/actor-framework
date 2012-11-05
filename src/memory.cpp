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


#include <vector>

#include "cppa/detail/memory.hpp"
#include "cppa/detail/recursive_queue_node.hpp"

using namespace std;

namespace cppa { namespace detail {

namespace {

pthread_key_t s_key;
pthread_once_t s_key_once = PTHREAD_ONCE_INIT;

constexpr size_t s_queue_node_storage_size = 20;
constexpr size_t s_max_cached_queue_nodes = 100;

} // namespace <anonymous>

class recursive_queue_node_storage : public ref_counted {

 public:

    recursive_queue_node_storage() {
        for (auto& instance : m_instances) {
            // each instance has a reference to its parent
            instance.parent = this;
            ref(); // deref() is called in memory::destroy
        }
    }

    typedef recursive_queue_node* iterator;

    iterator begin() { return std::begin(m_instances); }

    iterator end() { return std::end(m_instances); }

 private:

    recursive_queue_node m_instances[s_queue_node_storage_size];

};

class memory_cache {

 public:

    vector<recursive_queue_node*> qnodes;

    memory_cache() {
        qnodes.reserve(s_max_cached_queue_nodes);
    }

    ~memory_cache() {
        for (auto node : qnodes) memory::destroy(node);
    }

    static void destructor(void* ptr) {
        if (ptr) delete reinterpret_cast<memory_cache*>(ptr);
    }

    static void make() {
        pthread_key_create(&s_key, destructor);
    }

    static memory_cache* get() {
        pthread_once(&s_key_once, make);
        auto result = static_cast<memory_cache*>(pthread_getspecific(s_key));
        if (!result) {
            result = new memory_cache;
            pthread_setspecific(s_key, result);
        }
        return result;
    }

};

recursive_queue_node* memory::new_queue_node() {
    auto& vec = memory_cache::get()->qnodes;
    if (!vec.empty()) {
        recursive_queue_node* result = vec.back();
        vec.pop_back();
        return result;
    }
    auto storage = new recursive_queue_node_storage;
    for (auto i = storage->begin(); i != storage->end(); ++i) vec.push_back(i);
    return new_queue_node();
}

void memory::dispose(recursive_queue_node* ptr) {
    auto& vec = memory_cache::get()->qnodes;
    if (vec.size() < s_max_cached_queue_nodes) vec.push_back(ptr);
    else destroy(ptr);
}

void memory::destroy(recursive_queue_node* ptr) {
    auto parent = ptr->parent;
    parent->deref();
}

} } // namespace cppa::detail
