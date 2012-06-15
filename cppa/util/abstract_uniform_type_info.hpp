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


#ifndef CPPA_ABSTRACT_UNIFORM_TYPE_INFO_HPP
#define CPPA_ABSTRACT_UNIFORM_TYPE_INFO_HPP

#include "cppa/uniform_type_info.hpp"
#include "cppa/detail/to_uniform_name.hpp"

namespace cppa { namespace util {

/**
 * @brief Implements all pure virtual functions of {@link uniform_type_info}
 *        except serialize() and deserialize().
 */
template<typename T>
class abstract_uniform_type_info : public uniform_type_info {

    inline static const T& deref(const void* ptr) {
        return *reinterpret_cast<const T*>(ptr);
    }

    inline static T& deref(void* ptr) {
        return *reinterpret_cast<T*>(ptr);
    }

 protected:

    abstract_uniform_type_info(const std::string& uname
                           = detail::to_uniform_name(typeid(T)))
        : uniform_type_info(uname) {
    }

    bool equals(const void* lhs, const void* rhs) const {
        return deref(lhs) == deref(rhs);
    }

    void* new_instance(const void* ptr) const {
        return (ptr) ? new T(deref(ptr)) : new T();
    }

    void delete_instance(void* instance) const {
        delete reinterpret_cast<T*>(instance);
    }

 public:

    bool equals(const std::type_info& tinfo) const {
        return typeid(T) == tinfo;
    }

};

} }

#endif // CPPA_ABSTRACT_UNIFORM_TYPE_INFO_HPP
