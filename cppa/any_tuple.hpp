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


#ifndef ANY_TUPLE_HPP
#define ANY_TUPLE_HPP

#include "cppa/tuple.hpp"
#include "cppa/config.hpp"
#include "cppa/cow_ptr.hpp"
#include "cppa/detail/abstract_tuple.hpp"

namespace cppa {

/**
 * @brief Describes a fixed-length copy-on-write tuple
 *        with elements of any type.
 */
class any_tuple
{

 public:

    /**
     * @brief Creates an empty tuple.
     */
    any_tuple();

    /**
     * @brief Creates a tuple from @p t.
     */
    template<typename... Args>
    any_tuple(tuple<Args...> const& t) : m_vals(t.vals()) { }

    /**
     * @brief Creates a tuple and moves the content from @p t.
     */
    template<typename... Args>
    any_tuple(tuple<Args...>&& t) : m_vals(std::move(t.m_vals)) { }

    explicit any_tuple(detail::abstract_tuple*);

    /**
     * @brief Move constructor.
     */
    any_tuple(any_tuple&&);

    /**
     * @brief Copy constructor.
     */
    any_tuple(any_tuple const&) = default;

    /**
     * @brief Move assignment.
     */
    any_tuple& operator=(any_tuple&&);

    /**
     * @brief Copy assignment.
     */
    any_tuple& operator=(any_tuple const&) = default;

    /**
     * @brief Gets the size of this tuple.
     */
    size_t size() const;

    /**
     * @brief Gets a mutable pointer to the element at position @p p.
     */
    void* mutable_at(size_t p);

    /**
     * @brief Gets a const pointer to the element at position @p p.
     */
    void const* at(size_t p) const;

    /**
     * @brief Gets {@link uniform_type_info uniform type information}
     *        of the element at position @p p.
     */
    uniform_type_info const* type_at(size_t p) const;

    /**
     * @brief Returns @c true if <tt>*this == other</tt>, otherwise false.
     */
    bool equals(any_tuple const& other) const;

    /**
     * @brief Returns true if <tt>size() == 0</tt>, otherwise false.
     */
    inline bool empty() const { return size() == 0; }

    template<typename T>
    inline T const& get_as(size_t p) const
    {
        CPPA_REQUIRE(*(type_at(p)) == typeid(T));
        return *reinterpret_cast<T const*>(at(p));
    }

    template<typename T>
    inline T& get_mutable_as(size_t p)
    {
        CPPA_REQUIRE(*(type_at(p)) == typeid(T));
        return *reinterpret_cast<T*>(mutable_at(p));
    }

    typedef detail::abstract_tuple::const_iterator const_iterator;

    inline const_iterator begin() const { return m_vals->begin(); }

    inline const_iterator end() const { return m_vals->end(); }

    cow_ptr<detail::abstract_tuple> const& vals() const;

    inline void const* type_token() const
    {
        return m_vals->type_token();
    }

    inline std::type_info const* impl_type() const
    {
        return m_vals->impl_type();
    }

 private:

    cow_ptr<detail::abstract_tuple> m_vals;

    explicit any_tuple(cow_ptr<detail::abstract_tuple> const& vals);

};

inline bool operator==(any_tuple const& lhs, any_tuple const& rhs)
{
    return lhs.equals(rhs);
}

inline bool operator!=(any_tuple const& lhs, any_tuple const& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // ANY_TUPLE_HPP
