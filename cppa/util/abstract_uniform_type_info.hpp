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


#ifndef CPPA_ABSTRACT_UNIFORM_TYPE_INFO_HPP
#define CPPA_ABSTRACT_UNIFORM_TYPE_INFO_HPP

#include "cppa/deserializer.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"

namespace cppa { namespace util {

/**
 * @brief Implements all pure virtual functions of {@link uniform_type_info}
 *        except serialize() and deserialize().
 */
template<typename T>
class abstract_uniform_type_info : public uniform_type_info {

 public:

    bool equals(const std::type_info& tinfo) const {
        return typeid(T) == tinfo;
    }

    const char* name() const {
        return m_name.c_str();
    }

 protected:

    abstract_uniform_type_info() {
        auto uname = detail::to_uniform_name<T>();
        auto cname = detail::mapped_name_by_decorated_name(uname.c_str());
        if (cname == uname.c_str()) m_name = std::move(uname);
        else m_name = cname;
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

    void assert_type_name(deserializer* source) const {
        auto tname = source->seek_object();
        if (tname != name()) {
            std::string error_msg = "wrong type name found; expected \"";
            error_msg += name();
            error_msg += "\", found \"";
            error_msg += tname;
            error_msg += "\"";
            throw std::logic_error(std::move(error_msg));
        }
    }

 private:

    static inline const T& deref(const void* ptr) {
        return *reinterpret_cast<const T*>(ptr);
    }

    static inline T& deref(void* ptr) {
        return *reinterpret_cast<T*>(ptr);
    }

    std::string m_name;

};

} } // namespace cppa::util

#endif // CPPA_ABSTRACT_UNIFORM_TYPE_INFO_HPP
