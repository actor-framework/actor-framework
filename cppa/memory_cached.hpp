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


#ifndef CPPA_MEMORY_CACHED_HPP
#define CPPA_MEMORY_CACHED_HPP

#include <utility>
#include <type_traits>

#include "cppa/detail/memory.hpp"

namespace cppa {

/**
 * @brief This mixin adds all member functions and member variables needed
 *        by the memory management subsystem.
 */
template<class Base, class Subtype>
class memory_cached : public Base {

    friend class detail::memory;

    template<typename>
    friend class detail::basic_memory_cache;

 public:

    static constexpr bool is_memory_cached_type = true;

 protected:

    typedef memory_cached combined_type;

    template<typename... Ts>
    memory_cached(Ts&&... args) : Base(std::forward<Ts>(args)...)
                                , outer_memory(nullptr) { }

    virtual void request_deletion() {
        auto mc = detail::memory::get_cache_map_entry(&typeid(*this));
        if (!mc) {
            auto om = outer_memory;
            if (om) {
                om->destroy();
                om->deallocate();
            }
            else delete this;
        }
        else mc->release_instance(mc->downcast(this));
    }

 private:

    detail::instance_wrapper* outer_memory;

};

template<typename T>
struct is_memory_cached {
    template<class U, bool = U::is_memory_cached_type>
    static std::true_type check(int);
    template<class>
    static std::false_type check(...);
public:
    static constexpr bool value = decltype(check<T>(0))::value;
};

} // namespace cppa

#endif // CPPA_MEMORY_CACHED_HPP
