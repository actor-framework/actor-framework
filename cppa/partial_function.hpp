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


#ifndef PARTIAL_FUNCTION_HPP
#define PARTIAL_FUNCTION_HPP

#include <vector>
#include <memory>
#include <utility>

#include "cppa/detail/invokable.hpp"

namespace cppa {

class behavior;

class partial_function
{

 public:

    typedef std::unique_ptr<detail::invokable> invokable_ptr;

    partial_function() = default;

    partial_function(partial_function&& other);

    partial_function(invokable_ptr&& ptr);

    bool defined_at(any_tuple const& value);

    void operator()(any_tuple const& value);

    detail::invokable const* definition_at(any_tuple const& value);

    detail::intermediate* get_intermediate(any_tuple const& value);

    template<class... Args>
    partial_function& splice(partial_function&& arg0, Args&&... args)
    {
        auto& vec = arg0.m_funs;
        std::move(vec.begin(), vec.end(), std::back_inserter(m_funs));
        return splice(std::forward<Args>(args)...);
    }

    inline partial_function operator,(partial_function&& arg)
    {
        return std::move(splice(std::move(arg)));
    }

    behavior operator,(behavior&& arg);

 private:

    // terminates recursion
    inline partial_function& splice()
    {
        m_cache.clear();
        return *this;
    }

    typedef std::vector<detail::invokable*> cache_entry;
    typedef std::pair<void const*, cache_entry> cache_element;

    std::vector<invokable_ptr> m_funs;
    std::vector<cache_element> m_cache;
    cache_element m_dummy; // binary search dummy

    cache_entry& get_cache_entry(any_tuple const& value);

};

} // namespace cppa

#endif // PARTIAL_FUNCTION_HPP
