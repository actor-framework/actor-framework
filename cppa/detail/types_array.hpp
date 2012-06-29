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


#ifndef CPPA_TYPES_ARRAY_HPP
#define CPPA_TYPES_ARRAY_HPP

#include <typeinfo>

#include "cppa/util/type_list.hpp"
#include "cppa/util/is_builtin.hpp"

// forward declarations
namespace cppa {
class uniform_type_info;
const uniform_type_info* uniform_typeid(const std::type_info&);
} // namespace cppa

namespace cppa { namespace detail {

enum type_info_impl { std_tinf, cppa_tinf };

// some metaprogramming utility
template<type_info_impl What, bool IsBuiltin, typename T>
struct ta_util;

template<bool IsBuiltin, typename T>
struct ta_util<std_tinf, IsBuiltin, T> {
    static inline const std::type_info* get() { return &(typeid(T)); }
};

template<>
struct ta_util<std_tinf, true, anything> {
    static inline const std::type_info* get() { return nullptr; }
};

template<typename T>
struct ta_util<cppa_tinf, true, T> {
    static inline const uniform_type_info* get() {
        return uniform_typeid(typeid(T));
    }
};

template<>
struct ta_util<cppa_tinf, true, anything> {
    static inline const uniform_type_info* get() { return nullptr; }
};

template<typename T>
struct ta_util<cppa_tinf, false, T> {
    static inline const uniform_type_info* get() { return nullptr; }
};

// only built-in types are guaranteed to be available at static initialization
// time, other types are announced at runtime

// implements types_array
template<bool BuiltinOnlyTypes, typename... T>
struct types_array_impl {
    static constexpr bool builtin_only = true;
    inline bool is_pure() const { return true; }
    // all types are builtin, perform lookup on constuction
    const uniform_type_info* data[sizeof...(T)];
    types_array_impl() : data{ta_util<cppa_tinf, true, T>::get()...} { }
    inline const uniform_type_info* operator[](size_t p) const {
        return data[p];
    }
    typedef const uniform_type_info* const* const_iterator;
    inline const_iterator begin() const { return std::begin(data); }
    inline const_iterator end() const { return std::end(data); }
};

template<typename... T>
struct types_array_impl<false, T...> {
    static constexpr bool builtin_only = false;
    inline bool is_pure() const { return false; }
    // contains std::type_info for all non-builtin types
    const std::type_info* tinfo_data[sizeof...(T)];
    // contains uniform_type_infos for builtin types and lazy initializes
    // non-builtin types at runtime
    mutable std::atomic<const uniform_type_info*> data[sizeof...(T)];
    mutable std::atomic<const uniform_type_info* *> pairs;
    // pairs[sizeof...(T)];
    types_array_impl()
        : tinfo_data{ta_util<std_tinf,util::is_builtin<T>::value,T>::get()...} {
        bool static_init[sizeof...(T)] = {    !std::is_same<T,anything>::value
                                           && util::is_builtin<T>::value ...  };
        for (size_t i = 0; i < sizeof...(T); ++i) {
            if (static_init[i]) {
                data[i].store(uniform_typeid(*(tinfo_data[i])),
                              std::memory_order_relaxed);
            }
        }
    }
    inline const uniform_type_info* operator[](size_t p) const {
        auto result = data[p].load();
        if (result == nullptr) {
            auto tinfo = tinfo_data[p];
            if (tinfo != nullptr) {
                result = uniform_typeid(*tinfo);
                data[p].store(result, std::memory_order_relaxed);
            }
        }
        return result;
    }
    typedef const uniform_type_info* const* const_iterator;
    inline const_iterator begin() const {
        auto result = pairs.load();
        if (result == nullptr) {
            auto parr = new const uniform_type_info*[sizeof...(T)];
            for (size_t i = 0; i < sizeof...(T); ++i) {
                parr[i] = (*this)[i];
            }
            if (!pairs.compare_exchange_weak(result, parr, std::memory_order_relaxed)) {
                delete[] parr;
            }
            else {
                result = parr;
            }
        }
        return result;
    }
    inline const_iterator end() const {
        return begin() + sizeof...(T);
    }
};

// a container for uniform_type_information singletons with optimization
// for builtin types; can act as pattern
template<typename... T>
struct types_array : types_array_impl<util::tl_forall<util::type_list<T...>,
                                                      util::is_builtin>::value,
                                      T...> {
    static constexpr size_t size = sizeof...(T);
    typedef util::type_list<T...> types;
    typedef typename util::tl_filter_not<types, is_anything>::type
            filtered_types;
    static constexpr size_t filtered_size = filtered_types::size;
    inline bool has_values() const { return false; }
};

// utility for singleton-like access to a types_array
template<typename... T>
struct static_types_array {
    static types_array<T...> arr;
};

template<typename... T>
types_array<T...> static_types_array<T...>::arr;

template<typename TypeList>
struct static_types_array_from_type_list;

template<typename... T>
struct static_types_array_from_type_list<util::type_list<T...>> {
    typedef static_types_array<T...> type;
};

// utility for singleton-like access to a type_info instance of a type_list
template<typename... T>
struct static_type_list {
    static const std::type_info* list;
};

template<typename... T>
const std::type_info* static_type_list<T...>::list = &typeid(util::type_list<T...>);

} } // namespace cppa::detail

#endif // CPPA_TYPES_ARRAY_HPP
