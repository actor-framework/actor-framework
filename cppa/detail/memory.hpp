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
 * Copyright (C) 2011-2013                                                    *
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


#ifndef CPPA_MEMORY_HPP
#define CPPA_MEMORY_HPP

#include <new>
#include <vector>
#include <utility>
#include <typeinfo>
#include <iostream>

#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"

namespace cppa { namespace detail {

namespace {

constexpr size_t s_alloc_size = 1024;  // allocate ~1kb chunks
constexpr size_t s_cache_size = 10240; // cache about 10kb per thread

} // namespace <anonymous>

class recursive_queue_node;

class instance_wrapper {

 public:

    virtual ~instance_wrapper();

    // calls the destructor
    virtual void destroy() = 0;

    // releases memory
    virtual void deallocate() = 0;

};

class memory_cache {

 public:

    virtual ~memory_cache();

    // calls dtor and either releases memory or re-uses it later
    virtual void release_instance(void*) = 0;

    virtual std::pair<instance_wrapper*,void*> new_instance() = 0;

    // casts @p ptr to the derived type and returns it
    virtual void* downcast(memory_managed* ptr) = 0;

};

template<typename T>
class basic_memory_cache : public memory_cache {

    struct wrapper : instance_wrapper {
        ref_counted* parent;
        union { T instance; };
        wrapper() : parent(nullptr) { }
        ~wrapper() { }
        void destroy() { instance.~T(); }
        void deallocate() { parent->deref(); }
    };

    class storage : public ref_counted {

     public:

        storage() {
            for (auto& elem : data) {
                // each instance has a reference to its parent
                elem.parent = this;
                ref(); // deref() is called in wrapper::deallocate
            }
        }

        typedef wrapper* iterator;

        iterator begin() { return data; }

        iterator end() { return begin() + (s_alloc_size / sizeof(T)); }

     private:

        wrapper data[s_alloc_size / sizeof(T)];

    };

 public:

    std::vector<wrapper*> cached_elements;

    basic_memory_cache() {
        cached_elements.reserve(s_cache_size / sizeof(T));
    }

    ~basic_memory_cache() {
        for (auto e : cached_elements) e->deallocate();
    }

    void* downcast(memory_managed* ptr) {
        return static_cast<T*>(ptr);
    }


    virtual void release_instance(void* vptr) {
        CPPA_REQUIRE(vptr != nullptr);
        auto ptr = reinterpret_cast<T*>(vptr);
        CPPA_REQUIRE(ptr->outer_memory != nullptr);
        auto wptr = static_cast<wrapper*>(ptr->outer_memory);
        wptr->destroy();
        if (cached_elements.capacity() > 0) cached_elements.push_back(wptr);
        else wptr->deallocate();
    }

    virtual std::pair<instance_wrapper*,void*> new_instance() {
        if (cached_elements.empty()) {
            auto elements = new storage;
            for (auto i = elements->begin(); i != elements->end(); ++i) {
                cached_elements.push_back(i);
            }
        }
        wrapper* wptr = cached_elements.back();
        cached_elements.pop_back();
        return std::make_pair(wptr, &(wptr->instance));
    }

};

class memory {

    memory() = delete;

    template<typename>
    friend class basic_memory_cache;

 public:

    /*
     * @brief Allocates storage, initializes a new object, and returns
     *        the new instance.
     */
    template<typename T, typename... Args>
    static inline T* create(Args&&... args) {
        auto mc = get_or_set_cache_map_entry<T>();
        auto p = mc->new_instance();
        auto result = new (p.second) T (std::forward<Args>(args)...);
        result->outer_memory = p.first;
        return result;
    }

    static memory_cache* get_cache_map_entry(const std::type_info* tinf);

 private:

    static void add_cache_map_entry(const std::type_info* tinf, memory_cache* instance);

    template<typename T>
    static inline memory_cache* get_or_set_cache_map_entry() {
        auto mc = get_cache_map_entry(&typeid(T));
        if (!mc) {
            mc = new basic_memory_cache<T>;
            add_cache_map_entry(&typeid(T), mc);
        }
        return mc;
    }

};

struct disposer {
    inline void operator()(memory_managed* ptr) const {
        ptr->request_deletion();
    }
};

} } // namespace cppa::detail

#endif // CPPA_MEMORY_HPP
