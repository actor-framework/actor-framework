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


#ifndef MEMORY_CACHED_MIXIN_HPP
#define MEMORY_CACHED_MIXIN_HPP

#include "cppa/detail/memory.hpp"

namespace cppa {

/**
 * @brief This mixin adds all member functions and member variables needed
 *        by the memory management subsystem.
 */
template<typename Base>
class memory_cached_mixin : public Base {

    friend class detail::memory;

    template<typename T>
    friend class detail::basic_memory_cache;

 protected:

    template<typename... Args>
    memory_cached_mixin(Args&&... args)
    : Base(std::forward<Args>(args)...), outer_memory(nullptr) { }

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

} // namespace cppa

#endif // MEMORY_CACHED_MIXIN_HPP
