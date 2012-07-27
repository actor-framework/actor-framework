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


#ifndef CPPA_PARTIAL_FUNCTION_HPP
#define CPPA_PARTIAL_FUNCTION_HPP

#include <list>
#include <vector>
#include <memory>
#include <utility>

#include "cppa/any_tuple.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/timeout_definition.hpp"

#include "cppa/util/duration.hpp"

#include "cppa/detail/behavior_impl.hpp"

namespace cppa {

class behavior;

/**
 * @brief A partial function implementation
 *        for {@link cppa::any_tuple any_tuples}.
 */
class partial_function {

    friend class behavior;

 public:

    typedef intrusive_ptr<detail::behavior_impl> impl_ptr;

    partial_function() = default;
    partial_function(partial_function&&) = default;
    partial_function(const partial_function&) = default;
    partial_function& operator=(partial_function&&) = default;
    partial_function& operator=(const partial_function&) = default;

    partial_function(impl_ptr ptr);

    inline bool undefined() const {
        return m_impl == nullptr;
    }

    inline bool defined_at(const any_tuple& value) {
        return (m_impl) && m_impl->defined_at(value);
    }

    template<typename T>
    inline bool operator()(T&& arg) {
        return (m_impl) && m_impl->invoke(std::forward<T>(arg));
    }

 private:

    impl_ptr m_impl;

};

} // namespace cppa

#endif // CPPA_PARTIAL_FUNCTION_HPP
