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


#include "cppa/config.hpp"

#include <map>
#include <set>
#include <locale>
#include <string>
#include <atomic>
#include <limits>
#include <cstring>
#include <cstdint>
#include <type_traits>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/object.hpp"
#include "cppa/logging.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/announce.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/actor_addressing.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/duration.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

#include "cppa/message_header.hpp"

namespace cppa {

namespace { inline detail::uniform_type_info_map& uti_map() {
    return *detail::singleton_manager::get_uniform_type_info_map();
} } // namespace <anonymous>

const uniform_type_info* announce(const std::type_info&,
                                  std::unique_ptr<uniform_type_info> utype) {
    return uti_map().insert(std::move(utype));
}

uniform_type_info::~uniform_type_info() { }

object uniform_type_info::create() const {
    return {new_instance(), this};
}

const uniform_type_info* uniform_type_info::from(const std::type_info& tinf) {
    auto result = uti_map().by_rtti(tinf);
    if (result == nullptr) {
        std::string error = "uniform_type_info::by_type_info(): ";
        error += detail::to_uniform_name(tinf);
        error += " is an unknown typeid name";
        CPPA_LOGM_ERROR("cppa::uniform_type_info", error);
        throw std::runtime_error(error);
    }
    return result;
}

const uniform_type_info* uniform_type_info::from(const std::string& name) {
    auto result = uti_map().by_uniform_name(name);
    if (result == nullptr) {
        throw std::runtime_error(name + " is an unknown typeid name");
    }
    return result;
}

object uniform_type_info::deserialize(deserializer* from) const {
    auto ptr = new_instance();
    deserialize(ptr, from);
    return {ptr, this};
}

std::vector<const uniform_type_info*> uniform_type_info::instances() {
    return uti_map().get_all();
}

const uniform_type_info* uniform_typeid(const std::type_info& tinfo) {
    return uniform_type_info::from(tinfo);
}

} // namespace cppa
