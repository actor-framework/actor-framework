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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_ANNOUNCE_HPP
#define CPPA_ANNOUNCE_HPP

#include <memory>
#include <typeinfo>

#include "cppa/config.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/algorithm.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

#include "cppa/detail/default_uniform_type_info.hpp"

namespace cppa {

/**
 * @addtogroup TypeSystem
 * @{
 */

/**
 * @brief A simple example for @c announce with public accessible members.
 *
 * The output of this example program is:
 *
 * <tt>
 * foo(1, 2)<br>
 * foo_pair(3, 4)
 * </tt>
 * @example announce_1.cpp
 */

/**
 * @brief An example for @c announce with getter and setter member functions.
 *
 * The output of this example program is:
 *
 * <tt>foo(1, 2)</tt>
 * @example announce_2.cpp
 */

/**
 * @brief An example for @c announce with overloaded
 *        getter and setter member functions.
 *
 * The output of this example program is:
 *
 * <tt>foo(1, 2)</tt>
 * @example announce_3.cpp
 */

/**
 * @brief An example for @c announce with non-primitive members.
 *
 * The output of this example program is:
 *
 * <tt>bar(foo(1, 2), 3)</tt>
 * @example announce_4.cpp
 */

/**
 * @brief An advanced example for @c announce implementing serialization
 *        for a user-defined tree data type.
 * @example announce_5.cpp
 */

/**
 * @brief Adds a new type mapping to the libcppa type system.
 * @param tinfo C++ RTTI for the new type
 * @param utype Corresponding {@link uniform_type_info} instance.
 * @returns @c true if @p uniform_type was added as known
 *         instance (mapped to @p plain_type); otherwise @c false
 *         is returned and @p uniform_type was deleted.
 */
const uniform_type_info* announce(const std::type_info& tinfo,
                                  std::unique_ptr<uniform_type_info> utype);

// deals with member pointer
/**
 * @brief Creates meta informations for a non-trivial member @p C.
 * @param c_ptr Pointer to the non-trivial member.
 * @param args "Sub-members" of @p c_ptr
 * @see {@link announce_4.cpp announce example 4}
 * @returns A pair of @p c_ptr and the created meta informations.
 */
template<class C, class Parent, typename... Ts>
std::pair<C Parent::*, util::abstract_uniform_type_info<C>*>
compound_member(C Parent::*c_ptr, const Ts&... args) {
    return {c_ptr, new detail::default_uniform_type_info<C>(args...)};
}

// deals with getter returning a mutable reference
/**
 * @brief Creates meta informations for a non-trivial member accessed
 *        via a getter returning a mutable reference.
 * @param getter Member function pointer to the getter.
 * @param args "Sub-members" of @p c_ptr
 * @see {@link announce_4.cpp announce example 4}
 * @returns A pair of @p c_ptr and the created meta informations.
 */
template<class C, class Parent, typename... Ts>
std::pair<C& (Parent::*)(), util::abstract_uniform_type_info<C>*>
compound_member(C& (Parent::*getter)(), const Ts&... args) {
    return {getter, new detail::default_uniform_type_info<C>(args...)};
}

// deals with getter/setter pair
/**
 * @brief Creates meta informations for a non-trivial member accessed
 *        via a getter/setter pair.
 * @param gspair A pair of two member function pointers representing
 *               getter and setter.
 * @param args "Sub-members" of @p c_ptr
 * @see {@link announce_4.cpp announce example 4}
 * @returns A pair of @p c_ptr and the created meta informations.
 */
template<class Parent, typename GRes,
         typename SRes, typename SArg, typename... Ts>
std::pair<std::pair<GRes (Parent::*)() const, SRes (Parent::*)(SArg)>,
          util::abstract_uniform_type_info<typename util::rm_const_and_ref<GRes>::type>*>
compound_member(const std::pair<GRes (Parent::*)() const,
                                SRes (Parent::*)(SArg)  >& gspair,
                const Ts&... args) {
    typedef typename util::rm_const_and_ref<GRes>::type mtype;
    return {gspair, new detail::default_uniform_type_info<mtype>(args...)};
}

/**
 * @brief Adds a new type mapping for @p C to the libcppa type system.
 * @tparam C A class that is either empty or is default constructible,
 *           copy constructible, and comparable.
 * @param arg  First members of @p C.
 * @param args Additional members of @p C.
 * @warning @p announce is <b>not</b> thead safe!
 */
template<class C, typename... Ts>
inline const uniform_type_info* announce(const Ts&... args) {
    auto ptr = new detail::default_uniform_type_info<C>(args...);
    return announce(typeid(C), std::unique_ptr<uniform_type_info>{ptr});
}

/**
 * @}
 */

namespace detail {

template<long Pos, typename Tuple, typename T>
typename std::enable_if<util::is_primitive<T>::value>::type
serialze_single(serializer* sink, const Tuple&, const T& value) {
    sink->write_value(value);
}

template<long Pos, typename Tuple, typename T>
typename std::enable_if<not util::is_primitive<T>::value>::type
serialze_single(serializer* sink, const Tuple& tup, const T& value) {
    tup.type_at(Pos)->serialize(&value, sink);
}

// end of recursion
template<typename Tuple>
void do_serialize(serializer*, const Tuple&, util::int_list<>) { }

// end of recursion
template<typename Tuple, long I, long... Is>
void do_serialize(serializer* sink, const Tuple& tup, util::int_list<I, Is...>) {
    serialze_single<I>(sink, tup, get<I>(tup));
    do_serialize(sink, tup, util::int_list<Is...>{});
}

template<long Pos, typename Tuple, typename T>
typename std::enable_if<util::is_primitive<T>::value>::type
deserialze_single(deserializer* source, Tuple&, T& value) {
    value = source->read<T>();
}

template<long Pos, typename Tuple, typename T>
typename std::enable_if<not util::is_primitive<T>::value>::type
deserialze_single(deserializer* source, Tuple& tup, T& value) {
    tup.type_at(Pos)->deserialize(&value, source);
}

// end of recursion
template<typename Tuple>
void do_deserialize(deserializer*, const Tuple&, util::int_list<>) { }

// end of recursion
template<typename Tuple, long I, long... Is>
void do_deserialize(deserializer* source, Tuple& tup, util::int_list<I, Is...>) {
    deserialze_single<I>(source, tup, get_ref<I>(tup));
    do_deserialize(source, tup, util::int_list<Is...>{});
}

template<typename T, typename... Ts>
class meta_cow_tuple : public uniform_type_info {

 public:

    typedef cow_tuple<T, Ts...> tuple_type;

    meta_cow_tuple() {
        m_name = "@<>+";
        m_name += detail::to_uniform_name<T>();
        util::splice(m_name, "+", detail::to_uniform_name<Ts>()...);
    }

    const char* name() const override {
        return m_name.c_str();
    }

    void serialize(const void* instance, serializer* sink) const override {
        auto& ref = *cast(instance);
        do_serialize(sink, ref, util::get_indices(ref));
    }

    void deserialize(void* instance, deserializer* source) const override {
        auto& ref = *cast(instance);
        do_deserialize(source, ref, util::get_indices(ref));
    }

    void* new_instance(const void* other = nullptr) const override {
        return (other) ? new tuple_type{*cast(other)} : new tuple_type;
    }

    void delete_instance(void* instance) const override {
        delete cast(instance);
    }

    any_tuple as_any_tuple(void* instance) const override {
        return (instance) ? any_tuple{*cast(instance)} : any_tuple{};
    }

    bool equal_to(const std::type_info& tinfo) const override {
        return typeid(tuple_type) == tinfo;
    }

    bool equals(const void* instance1, const void* instance2) const override {
        return *cast(instance1) == *cast(instance2);
    }

 private:

    inline tuple_type* cast(void* ptr) const {
        return reinterpret_cast<tuple_type*>(ptr);
    }

    inline const tuple_type* cast(const void* ptr) const {
        return reinterpret_cast<const tuple_type*>(ptr);
    }

    std::string m_name;

};

} // namespace detail

/**
 * @addtogroup TypeSystem
 * @{
 */

/**
 * @brief Adds a hint to the type system of libcppa. This type hint can
 *        increase the network performance, because libcppa
 *        can use the hint to create tuples with full static type
 *        information rather than using fully dynamically typed tuples.
 */
template<typename T, typename... Ts>
inline void announce_tuple() {
    typedef detail::meta_cow_tuple<T, Ts...> meta_type;
    get_uniform_type_info_map()->insert(create_unique<meta_type>());
}

/**
 * @}
 */

} // namespace cppa

#endif // CPPA_ANNOUNCE_HPP
