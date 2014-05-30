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


#ifndef CPPA_OBJECT_HPP
#define CPPA_OBJECT_HPP

#include <string>
#include <typeinfo>
#include <stdexcept>
#include <type_traits>

#include "cppa/exception.hpp"
#include "cppa/util/type_traits.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa {

// forward declarations
class object;

/**
 * @relates object
 */
bool operator==(const object& lhs, const object& rhs);

class uniform_type_info;
const uniform_type_info* uniform_typeid(const std::type_info&);
bool operator==(const uniform_type_info& lhs, const std::type_info& rhs);

/**
 * @brief Grants mutable access to the stored value of @p obj.
 * @param obj {@link object Object} with <tt>obj.type() == typeid(T)</tt>.
 * @returns A mutable reference to the value stored in @p obj.
 * @relates object
 * @throws std::invalid_argument if <tt>obj.type() != typeid(T)</tt>
 */
template<typename T>
T& get_ref(object& obj);

/**
 * @brief Grants const access to the stored value of @p obj.
 * @param obj {@link object Object} with <tt>obj.type() == typeid(T)</tt>.
 * @returns A const reference to the value stored in @p obj.
 * @relates object
 * @throws std::invalid_argument if <tt>obj.type() != typeid(T)</tt>
 */
template<typename T>
const T& get(const object& obj);

/**
 * @brief An abstraction class that stores an instance of
 *        an announced type.
 */
class object {

    friend bool operator==(const object& lhs, const object& rhs);

 public:

    ~object();

    /**
     * @brief Creates an object of type @p utinfo with value @p val.
     * @warning {@link object} takes ownership of @p val.
     * @pre {@code val != nullptr && utinfo != nullptr}
     */
    object(void* val, const uniform_type_info* utinfo);

    /**
     * @brief Creates an empty object.
     * @post {type() == *uniform_typeid<unit_t>()}
     */
    object();

    /**
     * @brief Creates an object and moves type and value
     *        from @p other to @c this.
     */
    object(object&& other);

    /**
     * @brief Creates a (deep) copy of @p other.
     * @post {*this == other}
     */
    object(const object& other);

    /**
     * @brief Moves the content from @p other to this.
     * @returns @p *this
     */
    object& operator=(object&& other);

    /**
     * @brief Creates a (deep) copy of @p other and assigns it to @p this.
     * @returns @p *this
     */
    object& operator=(const object& other);

    /**
     * @brief Gets the RTTI of this object.
     * @returns A {@link uniform_type_info} describing the current
     *          type of @p this.
     */
    const uniform_type_info* type() const;

    /**
     * @brief Gets the stored value.
     * @returns A const pointer to the currently stored value.
     * @see get(const object&)
     */
    const void* value() const;

    /**
     * @brief Gets the stored value.
     * @returns A mutable pointer to the currently stored value.
     * @see get_ref(object&)
     */
    void* mutable_value();

    /**
     * @brief Creates an object from @p what.
     * @param what Value of an announced type.
     * @returns An object whose value was initialized with @p what.
     * @post {@code type() == *uniform_typeid<T>()}
     * @throws std::runtime_error if @p T is not announced.
     */
    template<typename T>
    static object from(T what);

 private:

    void* m_value;
    const uniform_type_info* m_type;

    void swap(object& other);

};

template<typename T>
object object::from(T what) {
    typedef typename util::rm_const_and_ref<T>::type plain_type;
    typedef typename detail::implicit_conversions<plain_type>::type value_type;
    auto rtti = uniform_typeid(typeid(value_type)); // throws on error
    return { new value_type(std::move(what)), rtti };
}

inline bool operator!=(const object& lhs, const object& rhs) {
    return !(lhs == rhs);
}

template<typename T>
T& get_ref(object& obj) {
    static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value,
                  "T is a reference or a pointer type.");
    if (!(*(obj.type()) == typeid(T))) {
        throw std::invalid_argument("obj.type() != typeid(T)");
    }
    return *reinterpret_cast<T*>(obj.mutable_value());
}

template<typename T>
const T& get(const object& obj) {
    static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value,
                  "T is a reference or a pointer type.");
    if (!(*(obj.type()) == typeid(T))) {
        throw std::invalid_argument("obj.type() != typeid(T)");
    }
    return *reinterpret_cast<const T*>(obj.value());
}

} // namespace cppa

#endif // CPPA_OBJECT_HPP
