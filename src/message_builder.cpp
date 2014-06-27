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

namespace {

class dynamic_msg_data : public detail::message_data {

    typedef message_data super;

 public:

    using message_data::const_iterator;

    dynamic_msg_data(const dynamic_msg_data& other) : super(true) {
        for (auto& d : other.m_data) {
            m_data.push_back(d->copy());
        }
    }

    dynamic_msg_data(std::vector<uniform_value>&& data)
            : super(true), m_data(std::move(data)) {}

    const void* at(size_t pos) const override {
        CPPA_REQUIRE(pos < size());
        return m_data[pos]->val;
    }

    void* mutable_at(size_t pos) override {
        CPPA_REQUIRE(pos < size());
        return m_data[pos]->val;
    }

    size_t size() const override { return m_data.size(); }

    dynamic_msg_data* copy() const override {
        return new dynamic_msg_data(*this);
    }

    const uniform_type_info* type_at(size_t pos) const override {
        CPPA_REQUIRE(pos < size());
        return m_data[pos]->ti;
    }

    const std::string* tuple_type_names() const override {
        return nullptr; // get_tuple_type_names(*this);
    }

 private:

    std::vector<uniform_value> m_data;

};

} // namespace <anonymous>

message_builder& message_builder::append(uniform_value what) {
    m_elements.push_back(std::move(what));
    return *this;
}

message message_builder::to_message() {
    return message{new dynamic_msg_data(std::move(m_elements))};
}

optional<message> message_builder::apply(message_handler handler) {
    return to_message().apply(std::move(handler));
}

} // namespace cppa
