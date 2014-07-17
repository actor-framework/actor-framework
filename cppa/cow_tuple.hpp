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

#ifndef CPPA_COW_TUPLE_HPP
#define CPPA_COW_TUPLE_HPP

// <backward_compatibility version="0.9" whole_file="yes">
#include <cstddef>
#include <string>
#include <typeinfo>
#include <type_traits>

#include "caf/message.hpp"
#include "caf/ref_counted.hpp"

#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/decorated_tuple.hpp"
#include "caf/detail/implicit_conversions.hpp"

namespace caf {

template<typename... Ts>
class cow_tuple;

/**
 * @ingroup CopyOnWrite
 * @brief A fixed-length copy-on-write tuple.
 */
template<typename Head, typename... Tail>
class cow_tuple<Head, Tail...> {

    static_assert(detail::tl_forall<detail::type_list<Head, Tail...>,
                                         detail::is_legal_tuple_type
                                        >::value,
                  "illegal types in cow_tuple definition: "
                  "pointers and references are prohibited");

    using data_type = detail::tuple_vals<
                        typename detail::strip_and_convert<Head>::type,
                        typename detail::strip_and_convert<Tail>::type...>;

    using data_ptr = detail::message_data::ptr;

 public:

    using types = detail::type_list<Head, Tail...>;

    static constexpr size_t num_elements = sizeof...(Tail) + 1;

    cow_tuple() : m_vals(new data_type) {
        // nop
    }

    /**
     * @brief Initializes the cow_tuple with @p args.
     * @param args Initialization values.
     */
    template<typename... Ts>
    cow_tuple(Head arg, Ts&&... args)
            : m_vals(new data_type(std::move(arg), std::forward<Ts>(args)...)) {
        // nop
    }

    cow_tuple(cow_tuple&&) = default;
    cow_tuple(const cow_tuple&) = default;
    cow_tuple& operator=(cow_tuple&&) = default;
    cow_tuple& operator=(const cow_tuple&) = default;

    /**
     * @brief Gets the size of this cow_tuple.
     */
    inline size_t size() const {
        return sizeof...(Tail) + 1;
    }

    /**
     * @brief Gets a const pointer to the element at position @p p.
     */
    inline const void* at(size_t p) const {
        return m_vals->at(p);
    }

    /**
     * @brief Gets a mutable pointer to the element at position @p p.
     */
    inline void* mutable_at(size_t p) {
        return m_vals->mutable_at(p);
    }

    /**
     * @brief Gets {@link uniform_type_info uniform type information}
     *        of the element at position @p p.
     */
    inline const uniform_type_info* type_at(size_t p) const {
        return m_vals->type_at(p);
    }

    inline cow_tuple<Tail...> drop_left() const {
        return cow_tuple<Tail...>::offset_subtuple(m_vals, 1);
    }

    inline operator message() {
        return message{m_vals};
    }

    static cow_tuple from(message& msg) {
        return cow_tuple(msg.vals());
    }

 private:

    cow_tuple(data_ptr ptr) : m_vals(ptr) { }

    data_type* ptr() {
        return static_cast<data_type*>(m_vals.get());
    }

    const data_type* ptr() const {
        return static_cast<data_type*>(m_vals.get());
    }

    data_ptr m_vals;

};

/**
 * @ingroup CopyOnWrite
 * @brief Gets a const-reference to the <tt>N</tt>th element of @p tup.
 * @param tup The cow_tuple object.
 * @returns A const-reference of type T, whereas T is the type of the
 *          <tt>N</tt>th element of @p tup.
 * @relates cow_tuple
 */
template<size_t N, typename... Ts>
const typename detail::type_at<N, Ts...>::type&
get(const cow_tuple<Ts...>& tup) {
    using result_type = typename detail::type_at<N, Ts...>::type;
    return *reinterpret_cast<const result_type*>(tup.at(N));
}

/**
 * @ingroup CopyOnWrite
 * @brief Gets a reference to the <tt>N</tt>th element of @p tup.
 * @param tup The cow_tuple object.
 * @returns A reference of type T, whereas T is the type of the
 *          <tt>N</tt>th element of @p tup.
 * @note Detaches @p tup if there are two or more references to the cow_tuple.
 * @relates cow_tuple
 */
template<size_t N, typename... Ts>
typename detail::type_at<N, Ts...>::type&
get_ref(cow_tuple<Ts...>& tup) {
    using result_type = typename detail::type_at<N, Ts...>::type;
    return *reinterpret_cast<result_type*>(tup.mutable_at(N));
}

/**
 * @ingroup ImplicitConversion
 * @brief Creates a new cow_tuple from @p args.
 * @param args Values for the cow_tuple elements.
 * @returns A cow_tuple object containing the values @p args.
 * @relates cow_tuple
 */
template<typename... Ts>
cow_tuple<typename detail::strip_and_convert<Ts>::type...>
make_cow_tuple(Ts&&... args) {
    return {std::forward<Ts>(args)...};
}

template<typename TypeList>
struct cow_tuple_from_type_list;

template<typename... Ts>
struct cow_tuple_from_type_list<detail::type_list<Ts...>> {
    typedef cow_tuple<Ts...> type;
};

} // namespace caf
// </backward_compatibility>

#endif // CPPA_COW_TUPLE_HPP
