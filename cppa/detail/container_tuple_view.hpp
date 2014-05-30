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


#ifndef CPPA_DETAIL_CONTAINER_TUPLE_VIEW_HPP
#define CPPA_DETAIL_CONTAINER_TUPLE_VIEW_HPP

#include <iostream>

#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/disablable_delete.hpp"

namespace cppa {
namespace detail {

template<class Container>
class container_tuple_view : public abstract_tuple {

    typedef abstract_tuple super;

    typedef typename Container::difference_type difference_type;

 public:

    typedef typename Container::value_type value_type;

    container_tuple_view(Container* c, bool take_ownership = false)
    : super(true), m_ptr(c) {
        CPPA_REQUIRE(c != nullptr);
        if (!take_ownership) m_ptr.get_deleter().disable();
    }

    size_t size() const override {
        return m_ptr->size();
    }

    abstract_tuple* copy() const override {
        return new container_tuple_view{new Container(*m_ptr), true};
    }

    const void* at(size_t pos) const override {
        CPPA_REQUIRE(pos < size());
        CPPA_REQUIRE(static_cast<difference_type>(pos) >= 0);
        auto i = m_ptr->cbegin();
        std::advance(i, static_cast<difference_type>(pos));
        return &(*i);
    }

    void* mutable_at(size_t pos) override {
        CPPA_REQUIRE(pos < size());
        CPPA_REQUIRE(static_cast<difference_type>(pos) >= 0);
        auto i = m_ptr->begin();
        std::advance(i, static_cast<difference_type>(pos));
        return &(*i);
    }

    const uniform_type_info* type_at(size_t) const override {
        return static_types_array<value_type>::arr[0];
    }

    const std::string* tuple_type_names() const override {
        static std::string result = demangle<value_type>();
        return &result;
    }

 private:

    std::unique_ptr<Container, disablable_delete> m_ptr;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_CONTAINER_TUPLE_VIEW_HPP
