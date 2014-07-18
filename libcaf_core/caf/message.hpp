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

#ifndef CAF_ANY_TUPLE_HPP
#define CAF_ANY_TUPLE_HPP

#include <type_traits>

#include "caf/config.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"

#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/implicit_conversions.hpp"

namespace caf {

class message_handler;

/**
 * @brief Describes a fixed-length copy-on-write tuple
 *        with elements of any type.
 */
class message {

 public:

    /**
     * @brief A raw pointer to the data.
     */
    using raw_ptr = detail::message_data*;

    /**
     * @brief A (COW) smart pointer to the data.
     */
    using data_ptr = detail::message_data::ptr;

    /**
     * @brief An iterator to access each element as <tt>const void*</tt>.
     */
    using const_iterator = detail::message_data::const_iterator;

    /**
     * @brief Creates an empty tuple.
     */
    message() = default;

    /**
     * @brief Move constructor.
     */
    message(message&&);

    /**
     * @brief Copy constructor.
     */
    message(const message&) = default;

    /**
     * @brief Move assignment.
     */
    message& operator=(message&&);

    /**
     * @brief Copy assignment.
     */
    message& operator=(const message&) = default;

    /**
     * @brief Gets the size of this tuple.
     */
    inline size_t size() const;

    /**
     * @brief Creates a new tuple with all but the first n values.
     */
    message drop(size_t n) const;

    /**
     * @brief Creates a new tuple with all but the last n values.
     */
    message drop_right(size_t n) const;

    /**
     * @brief Creates a new tuple from the first n values.
     */
    inline message take(size_t n) const;

    /**
     * @brief Creates a new tuple from the last n values.
     */
    inline message take_right(size_t n) const;

    /**
     * @brief Gets a mutable pointer to the element at position @p p.
     */
    void* mutable_at(size_t p);

    /**
     * @brief Gets a const pointer to the element at position @p p.
     */
    const void* at(size_t p) const;

    /**
     * @brief Gets {@link uniform_type_info uniform type information}
     *        of the element at position @p p.
     */
    const uniform_type_info* type_at(size_t p) const;

    /**
     * @brief Returns true if this message has the types @p Ts.
     */
    template<typename... Ts>
    bool has_types() const;

    /**
     * @brief Returns @c true if <tt>*this == other</tt>, otherwise false.
     */
    bool equals(const message& other) const;

    /**
     * @brief Returns true if <tt>size() == 0</tt>, otherwise false.
     */
    inline bool empty() const;

    /**
     * @brief Returns the value at @p as instance of @p T.
     */
    template<typename T>
    inline const T& get_as(size_t p) const;

    /**
     * @brief Returns the value at @p as mutable data_ptr& of type @p T&.
     */
    template<typename T>
    inline T& get_as_mutable(size_t p);

    /**
     * @brief Returns an iterator to the beginning.
     */
    inline const_iterator begin() const;

    /**
     * @brief Returns an iterator to the end.
     */
    inline const_iterator end() const;

    /**
     * @brief Returns a copy-on-write pointer to the internal data.
     */
    inline data_ptr& vals();

    /**
     * @brief Returns a const copy-on-write pointer to the internal data.
     */
    inline const data_ptr& vals() const;

    /**
     * @brief Returns a const copy-on-write pointer to the internal data.
     */
    inline const data_ptr& cvals() const;

    /**
     * @brief Returns either <tt>&typeid(detail::type_list<Ts...>)</tt>, where
     *        <tt>Ts...</tt> are the element types, or <tt>&typeid(void)</tt>.
     *
     * The type token @p &typeid(void) indicates that this tuple is dynamically
     * typed, i.e., the types where not available at compile time.
     */
    inline const std::type_info* type_token() const;

    /**
     * @brief Checks whether this tuple is dynamically typed, i.e.,
     *        its types were not known at compile time.
     */
    inline bool dynamically_typed() const;

    /**
     * @brief Applies @p handler to this message and returns the result
     *        of <tt>handler(*this)</tt>.
     */
    optional<message> apply(message_handler handler);

    /** @cond PRIVATE */

    inline void force_detach();

    void reset();

    explicit message(raw_ptr);

    inline const std::string* tuple_type_names() const;

    explicit message(const data_ptr& vals);

    template<typename... Ts>
    static inline message move_from_tuple(std::tuple<Ts...>&&);

    /** @endcond */

 private:

    data_ptr m_vals;

};

/**
 * @relates message
 */
inline bool operator==(const message& lhs, const message& rhs) {
    return lhs.equals(rhs);
}

/**
 * @relates message
 */
inline bool operator!=(const message& lhs, const message& rhs) {
    return !(lhs == rhs);
}

/**
 * @brief Creates an {@link message} containing the elements @p args.
 * @param args Values to initialize the tuple elements.
 */
template<typename T, typename... Ts>
typename std::enable_if<
    !std::is_same<message, typename detail::rm_const_and_ref<T>::type>::value ||
        (sizeof...(Ts) > 0),
    message>::type
make_message(T&& arg, Ts&&... args) {
    using namespace detail;
    using data = tuple_vals<typename strip_and_convert<T>::type,
                            typename strip_and_convert<Ts>::type...>;
    auto ptr = new data(std::forward<T>(arg), std::forward<Ts>(args)...);
    return message{detail::message_data::ptr{ptr}};
}

inline message make_message(message other) { return std::move(other); }

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline bool message::empty() const { return size() == 0; }

template<typename T>
inline const T& message::get_as(size_t p) const {
    CAF_REQUIRE(*(type_at(p)) == typeid(T));
    return *reinterpret_cast<const T*>(at(p));
}

template<typename T>
inline T& message::get_as_mutable(size_t p) {
    CAF_REQUIRE(*(type_at(p)) == typeid(T));
    return *reinterpret_cast<T*>(mutable_at(p));
}

inline message::const_iterator message::begin() const {
    return m_vals->begin();
}

inline message::const_iterator message::end() const { return m_vals->end(); }

inline message::data_ptr& message::vals() { return m_vals; }

inline const message::data_ptr& message::vals() const { return m_vals; }

inline const message::data_ptr& message::cvals() const { return m_vals; }

inline const std::type_info* message::type_token() const {
    return m_vals->type_token();
}

inline bool message::dynamically_typed() const {
    return m_vals->dynamically_typed();
}

inline void message::force_detach() { m_vals.detach(); }

inline const std::string* message::tuple_type_names() const {
    return m_vals->tuple_type_names();
}

inline size_t message::size() const { return m_vals ? m_vals->size() : 0; }

inline message message::take(size_t n) const {
    return n >= size() ? *this : drop_right(size() - n);
}

inline message message::take_right(size_t n) const {
    return n >= size() ? *this : drop(size() - n);
}

struct move_from_tuple_helper {
    template<typename... Ts>
    inline message operator()(Ts&... vs) {
        return make_message(std::move(vs)...);
    }

};

template<typename... Ts>
inline message message::move_from_tuple(std::tuple<Ts...>&& tup) {
    move_from_tuple_helper f;
    return detail::apply_args(f, detail::get_indices(tup), tup);
}

template<typename... Ts>
bool message::has_types() const {
    if (size() != sizeof...(Ts)) return false;
    const std::type_info* ts[] = {&typeid(Ts)...};
    for (size_t i = 0; i < sizeof...(Ts); ++i) {
        if (!type_at(i)->equal_to(*ts[i])) return false;
    }
    return true;
}

} // namespace caf

#endif // CAF_ANY_TUPLE_HPP
