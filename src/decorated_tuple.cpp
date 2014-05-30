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


#include "cppa/detail/decorated_tuple.hpp"

namespace cppa {
namespace detail {

void* decorated_tuple::mutable_at(size_t pos) {
    CPPA_REQUIRE(pos < size());
    return m_decorated->mutable_at(m_mapping[pos]);
}

size_t decorated_tuple::size() const {
    return m_mapping.size();
}

decorated_tuple* decorated_tuple::copy() const {
    return new decorated_tuple(*this);
}

const void* decorated_tuple::at(size_t pos) const {
    CPPA_REQUIRE(pos < size());
    return m_decorated->at(m_mapping[pos]);
}

const uniform_type_info* decorated_tuple::type_at(size_t pos) const {
    CPPA_REQUIRE(pos < size());
    return m_decorated->type_at(m_mapping[pos]);
}

auto decorated_tuple::type_token() const -> rtti {
    return m_token;
}

void decorated_tuple::init() {
    CPPA_REQUIRE(   m_mapping.empty()
                 ||   *(std::max_element(m_mapping.begin(), m_mapping.end()))
                    < static_cast<const pointer&>(m_decorated)->size());
}

void decorated_tuple::init(size_t offset) {
    const pointer& dec = m_decorated;
    if (offset < dec->size()) {
        size_t i = offset;
        m_mapping.resize(dec->size() - offset);
        std::generate(m_mapping.begin(), m_mapping.end(), [&] {return i++;});
    }
    init();
}

decorated_tuple::decorated_tuple(pointer d, vector_type&& v)
: super(true), m_decorated(std::move(d)), m_token(&typeid(void)), m_mapping(std::move(v)) {
    init();
}

decorated_tuple::decorated_tuple(pointer d, rtti ti, vector_type&& v)
: super(false), m_decorated(std::move(d)), m_token(ti), m_mapping(std::move(v)) {
    init();
}

decorated_tuple::decorated_tuple(pointer d, size_t offset)
: super(true), m_decorated(std::move(d)), m_token(&typeid(void)) {
    init(offset);
}

decorated_tuple::decorated_tuple(pointer d, rtti ti, size_t offset)
: super(false), m_decorated(std::move(d)), m_token(ti) {
    init(offset);
}

const std::string* decorated_tuple::tuple_type_names() const {
    // produced name is equal for all instances
    static std::string result = get_tuple_type_names(*this);
    return &result;
}

} // namespace detail
} // namespace cppa

