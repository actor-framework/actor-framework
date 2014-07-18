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

#ifndef CAF_DETAIL_TYPES_ARRAY_HPP
#define CAF_DETAIL_TYPES_ARRAY_HPP

#include <atomic>
#include <typeinfo>

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"

// forward declarations
namespace caf {
class uniform_type_info;
const uniform_type_info* uniform_typeid(const std::type_info&);
} // namespace caf

namespace caf {
namespace detail {

enum type_info_impl {
    std_tinf,
    caf_tinf
};

// some metaprogramming utility
template<type_info_impl What, bool IsBuiltin, typename T>
struct ta_util;

template<bool IsBuiltin, typename T>
struct ta_util<std_tinf, IsBuiltin, T> {
    static inline const std::type_info* get() {
        return &(typeid(T));
    }
};

template<>
struct ta_util<std_tinf, true, anything> {
    static inline const std::type_info* get() {
        return nullptr;
    }
};

template<typename T>
struct ta_util<caf_tinf, true, T> {
    static inline const uniform_type_info* get() {
        return uniform_typeid(typeid(T));
    }
};

template<>
struct ta_util<caf_tinf, true, anything> {
    static inline const uniform_type_info* get() {
        return nullptr;
    }
};

template<typename T>
struct ta_util<caf_tinf, false, T> {
    static inline const uniform_type_info* get() {
        return nullptr;
    }
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
    types_array_impl() : data{ta_util<caf_tinf, true, T>::get()...} {}
    inline const uniform_type_info* operator[](size_t p) const {
        return data[p];
    }
    using const_iterator = const uniform_type_info* const*;
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
    mutable std::atomic<const uniform_type_info**> pairs;
    // pairs[sizeof...(T)];
    types_array_impl()
            : tinfo_data{ta_util<std_tinf, is_builtin<T>::value, T>::get()...} {
        bool static_init[sizeof...(T)] = {!std::is_same<T, anything>::value &&
                                          is_builtin<T>::value...};
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
    using const_iterator = const uniform_type_info* const*;
    inline const_iterator begin() const {
        auto result = pairs.load();
        if (result == nullptr) {
            auto parr = new const uniform_type_info* [sizeof...(T)];
            for (size_t i = 0; i < sizeof...(T); ++i) {
                parr[i] = (*this)[i];
            }
            if (!pairs.compare_exchange_weak(result, parr,
                                             std::memory_order_relaxed)) {
                delete[] parr;
            } else {
                result = parr;
            }
        }
        return result;
    }
    inline const_iterator end() const { return begin() + sizeof...(T); }

};

// a container for uniform_type_information singletons with optimization
// for builtin types; can act as pattern
template<typename... T>
struct types_array
    : types_array_impl<
          tl_forall<type_list<T...>, is_builtin>::value,
          T...> {
    static constexpr size_t size = sizeof...(T);
    using types = type_list<T...>;
    using filtered_types = typename tl_filter_not<
                               types,
                               is_anything
                           >::type;
    static constexpr size_t filtered_size =
        tl_size<filtered_types>::value;
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
struct static_types_array_from_type_list<type_list<T...>> {
    using type = static_types_array<T...>;

};

template<typename... T>
struct static_type_list;

template<typename T>
struct static_type_list<T> {
    static const std::type_info* list;
    static inline const std::type_info* by_offset(size_t offset) {
        return offset == 0 ? list : &typeid(type_list<>);
    }
};

template<typename T>
const std::type_info* static_type_list<T>::list = &typeid(type_list<T>);

// utility for singleton-like access to a type_info instance of a type_list
template<typename T0, typename T1, typename... Ts>
struct static_type_list<T0, T1, Ts...> {
    static const std::type_info* list;
    static inline const std::type_info* by_offset(size_t offset) {
        return offset == 0 ? list : static_type_list<T1, Ts...>::list;
    }
};

template<typename T0, typename T1, typename... Ts>
const std::type_info* static_type_list<T0, T1, Ts...>::list =
    &typeid(type_list<T0, T1, Ts...>);

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TYPES_ARRAY_HPP
