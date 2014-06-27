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
#include "cppa/logging.hpp"
#include "cppa/message.hpp"
#include "cppa/announce.hpp"
#include "cppa/message.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/duration.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"

namespace cppa {

namespace { inline detail::uniform_type_info_map& uti_map() {
    return *detail::singleton_manager::get_uniform_type_info_map();
} } // namespace <anonymous>

const uniform_type_info* announce(const std::type_info&,
                                  std::unique_ptr<uniform_type_info> utype) {
    return uti_map().insert(std::move(utype));
}

uniform_type_info::~uniform_type_info() {
    // nop
}

uniform_value_t::~uniform_value_t() {
    // nop
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

uniform_value uniform_type_info::deserialize(deserializer* from) const {
    auto uval = create();
    deserialize(uval->val, from);
    return uval;
}

std::vector<const uniform_type_info*> uniform_type_info::instances() {
    return uti_map().get_all();
}

const uniform_type_info* uniform_typeid(const std::type_info& tinfo) {
    return uniform_type_info::from(tinfo);
}

} // namespace cppa
