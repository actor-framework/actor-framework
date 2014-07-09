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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CPPA_UNIFORM_TYPE_INFO_MAP_HPP
#define CPPA_UNIFORM_TYPE_INFO_MAP_HPP

#include <set>
#include <map>
#include <string>
#include <utility>
#include <type_traits>

#include "cppa/fwd.hpp"

#include "cppa/atom.hpp"
#include "cppa/unit.hpp"
#include "cppa/node_id.hpp"
#include "cppa/duration.hpp"
#include "cppa/accept_handle.hpp"
#include "cppa/system_messages.hpp"
#include "cppa/connection_handle.hpp"

#include "cppa/detail/type_list.hpp"

#include "cppa/detail/singleton_mixin.hpp"

namespace cppa {
class uniform_type_info;
} // namespace cppa

namespace cppa {
namespace detail {

const char* mapped_name_by_decorated_name(const char* decorated_name);

std::string mapped_name_by_decorated_name(std::string&& decorated_name);

inline const char*
mapped_name_by_decorated_name(const std::string& decorated_name) {
    return mapped_name_by_decorated_name(decorated_name.c_str());
}

// lookup table for integer types
extern const char* mapped_int_names[][2];

template<typename T>
constexpr const char* mapped_int_name() {
    return mapped_int_names[sizeof(T)][std::is_signed<T>::value ? 1 : 0];
}

class uniform_type_info_map_helper;

// note: this class is implemented in uniform_type_info.cpp
class uniform_type_info_map {

    friend class uniform_type_info_map_helper;
    friend class singleton_mixin<uniform_type_info_map>;

 public:

    using pointer = const uniform_type_info*;

    virtual ~uniform_type_info_map();

    virtual pointer by_uniform_name(const std::string& name) = 0;

    virtual pointer by_rtti(const std::type_info& ti) const = 0;

    virtual std::vector<pointer> get_all() const = 0;

    virtual pointer insert(uniform_type_info_ptr uti) = 0;

    static uniform_type_info_map* create_singleton();

    inline void dispose() { delete this; }

    inline void stop() { }

    virtual void initialize() = 0;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_UNIFORM_TYPE_INFO_MAP_HPP
