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


#ifndef ADD_TUPLE_HINT_HPP
#define ADD_TUPLE_HINT_HPP

#include "cppa/config.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/singletons.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/util/algorithm.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"

namespace cppa {

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

    bool equals(const std::type_info& tinfo) const {
        return typeid(tuple_type) == tinfo;
    }

    bool equals(const void* instance1, const void* instance2) const {
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
 *        significantly increase the network performance, because libcppa
 *        can use the hint to create tuples with full static type information
 *        rather than using fully dynamically typed tuples.
 */
template<typename T, typename... Ts>
inline void add_tuple_hint() {
    typedef detail::meta_cow_tuple<T, Ts...> meta_type;
    get_uniform_type_info_map()->insert(create_unique<meta_type>());
}

/**
 * @}
 */

} // namespace cppa

#endif // ADD_TUPLE_HINT_HPP
