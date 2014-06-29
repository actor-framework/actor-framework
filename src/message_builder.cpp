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

#include <vector>

#include "cppa/message_builder.hpp"
#include "cppa/message_handler.hpp"
#include "cppa/uniform_type_info.hpp"

namespace cppa {

class message_builder::dynamic_msg_data : public detail::message_data {

    using super = message_data;

 public:

    using message_data::const_iterator;

    dynamic_msg_data() : super(true) { }

    dynamic_msg_data(const dynamic_msg_data& other) : super(true) {
        for (auto& d : other.m_elements) {
            m_elements.push_back(d->copy());
        }
    }

    dynamic_msg_data(std::vector<uniform_value>&& data)
            : super(true), m_elements(std::move(data)) { }

    const void* at(size_t pos) const override {
        CPPA_REQUIRE(pos < size());
        return m_elements[pos]->val;
    }

    void* mutable_at(size_t pos) override {
        CPPA_REQUIRE(pos < size());
        return m_elements[pos]->val;
    }

    size_t size() const override {
        return m_elements.size();
    }

    dynamic_msg_data* copy() const override {
        return new dynamic_msg_data(*this);
    }

    const uniform_type_info* type_at(size_t pos) const override {
        CPPA_REQUIRE(pos < size());
        return m_elements[pos]->ti;
    }

    const std::string* tuple_type_names() const override {
        return nullptr; // get_tuple_type_names(*this);
    }

    std::vector<uniform_value> m_elements;

};

message_builder::message_builder() {
    init();
}

message_builder::~message_builder() {
    // nop
}

void message_builder::init() {
    // this should really be done by delegating
    // constructors, but we want to support
    // some compilers without that feature...
    m_data.reset(new dynamic_msg_data);
}

void message_builder::clear() {
    data()->m_elements.clear();
}

size_t message_builder::size() const {
    return data()->m_elements.size();
}

bool message_builder::empty() const {
    return size() == 0;
}

message_builder& message_builder::append(uniform_value what) {
    data()->m_elements.push_back(std::move(what));
    return *this;
}

message message_builder::to_message() {
    return message{data()};
}

message_builder::dynamic_msg_data* message_builder::data() {
    // detach if needed, i.e., assume further non-const
    // operations on m_data can cause race conditions if
    // someone else holds a reference to m_data
    if (!m_data->unique()) {
        intrusive_ptr<ref_counted> tmp{std::move(m_data)};
        m_data.reset(static_cast<dynamic_msg_data*>(tmp.get())->copy());
    }
    return static_cast<dynamic_msg_data*>(m_data.get());
}

const message_builder::dynamic_msg_data* message_builder::data() const {
    return static_cast<const dynamic_msg_data*>(m_data.get());
}

} // namespace cppa
