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


#ifndef CPPA_TDATA_HPP
#define CPPA_TDATA_HPP

#include <typeinfo>
#include <functional>
#include <type_traits>

#include "cppa/get.hpp"
#include "cppa/option.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/wrapped.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/arg_match_t.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/tuple_iterator.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa {
class uniform_type_info;
const uniform_type_info* uniform_typeid(const std::type_info&);
} // namespace cppa

namespace cppa { namespace detail {

template<typename T>
inline void* ptr_to(T& what) { return &what; }

template<typename T>
inline const void* ptr_to(const T& what) { return &what; }

template<typename T>
inline void* ptr_to(T* what) { return what; }

template<typename T>
inline const void* ptr_to(const T* what) { return what; }

template<typename T>
inline void* ptr_to(const std::reference_wrapper<T>& what) {
    return &(what.get());
}

template<typename T>
inline const void* ptr_to(const std::reference_wrapper<const T>& what) {
    return &(what.get());
}

template<typename T>
inline const uniform_type_info* utype_of(const T&) {
    return static_types_array<T>::arr[0];
}

template<typename T>
inline const uniform_type_info* utype_of(const std::reference_wrapper<T>&) {
    return static_types_array<typename util::rm_ref<T>::type>::arr[0];
}

template<typename T>
inline const uniform_type_info* utype_of(const T* ptr) {
    return utype_of(*ptr);
}

template<typename T>
struct boxed_or_void {
    static constexpr bool value = is_boxed<T>::value;
};

template<>
struct boxed_or_void<util::void_type> {
    static constexpr bool value = true;
};

template<typename T>
struct unbox_ref {
    typedef T type;
};

template<typename T>
struct unbox_ref<std::reference_wrapper<T> > {
    typedef T type;
};

/*
 * "enhanced" std::tuple
 */
template<typename...>
struct tdata;

template<>
struct tdata<> {

    typedef tdata super;

    util::void_type head;

    typedef util::void_type head_type;
    typedef tdata<> tail_type;
    typedef util::void_type back_type;
    typedef util::type_list<> types;

    static constexpr size_t num_elements = 0;

    constexpr tdata() { }

    // swallow any number of additional boxed or void_type arguments silently
    template<typename... Args>
    tdata(Args&&...) {
        typedef util::type_list<typename util::rm_ref<Args>::type...> incoming;
        typedef typename util::tl_filter_not_type<incoming, tdata>::type iargs;
        static_assert(util::tl_forall<iargs, boxed_or_void>::value,
                      "Additional unboxed arguments provided");
    }

    typedef tuple_iterator<tdata> const_iterator;

    inline const_iterator begin() const { return {this}; }
    inline const_iterator cbegin() const { return {this}; }

    inline const_iterator end() const { return {this}; }
    inline const_iterator cend() const { return {this}; }

    inline size_t size() const { return num_elements; }

    tdata<>& tail() { return *this; }

    const tdata<>& tail() const { return *this; }

    const tdata<>& ctail() const { return *this; }

    inline const void* at(size_t) const {
        throw std::out_of_range("tdata<>");
    }

    inline void* mutable_at(size_t) {
        throw std::out_of_range("tdata<>");
    }

    inline const uniform_type_info* type_at(size_t) const {
        throw std::out_of_range("tdata<>");
    }

    inline void set() { }

    inline bool operator==(const tdata&) const { return true; }

    inline tuple_impl_info impl_type() const {
        return statically_typed;
    }

};

template<bool IsBoxed, bool IsFunction, typename Head, typename T>
struct td_filter_ {
    static inline const T& _(const T& arg) { return arg; }
    static inline T&& _(T&& arg) { return std::move(arg); }
    static inline T& _(T& arg) { return arg; }
};

template<typename Head, typename T>
struct td_filter_<false, true, Head, T> {
    static inline T* _(T* arg) { return arg; }
};

template<typename Head, typename T>
struct td_filter_<true, false, Head, T> {
    static inline Head _(const T&) { return Head{}; }
};

template<typename Head, typename T>
struct td_filter_<true, true, Head, T> : td_filter_<true, false, Head, T> {
};

template<typename Head, typename T>
auto td_filter(T&& arg)
    -> decltype(
        td_filter_<
            is_boxed<typename util::rm_ref<T>::type>::value,
            std::is_function<typename util::rm_ref<T>::type>::value,
            Head,
            typename util::rm_ref<T>::type
        >::_(std::forward<T>(arg))) {
    return  td_filter_<
                is_boxed<typename util::rm_ref<T>::type>::value,
                std::is_function<typename util::rm_ref<T>::type>::value,
                Head,
                typename util::rm_ref<T>::type
            >::_(std::forward<T>(arg));
}

template<typename... X, typename... Y>
void tdata_set(tdata<X...>& rhs, const tdata<Y...>& lhs);

template<typename Head, typename... Tail>
struct tdata<Head, Tail...> : tdata<Tail...> {

    typedef tdata<Tail...> super;

    typedef util::type_list<
                typename unbox_ref<Head>::type,
                typename unbox_ref<Tail>::type...>
            types;

    Head head;

    static constexpr size_t num_elements = (sizeof...(Tail) + 1);

    typedef Head head_type;
    typedef tdata<Tail...> tail_type;

    typedef typename util::if_else_c< (sizeof...(Tail) > 0),
                typename tdata<Tail...>::back_type,
                util::wrapped<Head>
            >::type
            back_type;

    inline tdata() : super(), head() { }

    tdata(Head arg) : super(), head(std::move(arg)) { }

    template<typename Arg0, typename Arg1, typename... Args>
    tdata(Arg0&& arg0, Arg1&& arg1, Args&&... args)
        : super(std::forward<Arg1>(arg1), std::forward<Args>(args)...)
        , head(td_filter<Head>(std::forward<Arg0>(arg0))) {
    }

    tdata(const tdata&) = default;

    tdata(tdata&& other)
        : super(std::move(other.tail())), head(std::move(other.head)) {
    }

    // allow (partial) initialization from a different tdata

    template<typename... Y>
    tdata(tdata<Y...>& other) : super(other.tail()), head(other.head) {
    }

    template<typename... Y>
    tdata(const tdata<Y...>& other) : super(other.tail()), head(other.head) {
    }

    template<typename... Y>
    tdata(tdata<Y...>&& other)
        : super(std::move(other.tail())), head(std::move(other.head)) {
    }

    template<typename... Y>
    tdata& operator=(const tdata<Y...>& other) {
        tdata_set(*this, other);
        return *this;
    }

    template<typename Arg0, typename... Args>
    inline void set(Arg0&& arg0, Args&&... args) {
        head = std::forward<Arg0>(arg0);
        super::set(std::forward<Args>(args)...);
    }

    inline size_t size() const { return num_elements; }

    typedef tuple_iterator<tdata> const_iterator;

    inline const_iterator begin() const { return {this}; }
    inline const_iterator cbegin() const { return {this}; }

    inline const_iterator end() const { return {this, size()}; }
    inline const_iterator cend() const { return {this, size()}; }


    // upcast
    inline tdata<Tail...>& tail() { return *this; }

    inline const tdata<Tail...>& tail() const { return *this; }

    inline const tdata<Tail...>& ctail() const { return *this; }

    inline const void* at(size_t p) const {
        CPPA_REQUIRE(p < num_elements);
        switch (p) {
            case  0: return ptr_to(head);
            case  1: return ptr_to(super::head);
            case  2: return ptr_to(super::super::head);
            case  3: return ptr_to(super::super::super::head);
            case  4: return ptr_to(super::super::super::super::head);
            case  5: return ptr_to(super::super::super::super::super::head);
            default: return super::at(p - 1);
        }
        //return (p == 0) ? ptr_to(head) : super::at(p - 1);
    }

    inline void* mutable_at(size_t p) {
#       ifdef CPPA_DEBUG
        if (p == 0) {
            if (std::is_same<decltype(ptr_to(head)), const void*>::value) {
                throw std::logic_error{"mutable_at with const head"};
            }
            return const_cast<void*>(ptr_to(head));
        }
        return super::mutable_at(p - 1);
#       else
        return const_cast<void*>(at(p));
#       endif

    }

    inline const uniform_type_info* type_at(size_t p) const {
        return (p == 0) ? utype_of(head) : super::type_at(p-1);
    }

    Head& _back(std::integral_constant<size_t, 0>) {
        return head;
    }

    template<size_t Pos>
    back_type& _back(std::integral_constant<size_t, Pos>) {
        std::integral_constant<size_t, Pos - 1> token;
        return super::_back(token);
    }

    back_type& back() {
        std::integral_constant<size_t, sizeof...(Tail)> token;
        return _back(token);
    }

    const Head& _back(std::integral_constant<size_t, 0>) const {
        return head;
    }

    template<size_t Pos>
    const back_type& _back(std::integral_constant<size_t, Pos>) const {
        std::integral_constant<size_t, Pos - 1> token;
        return super::_back(token);
    }

    const back_type& back() const {
        std::integral_constant<size_t, sizeof...(Tail)> token;
        return _back(token);
    }
};

template<typename... X>
void tdata_set(tdata<X...>&, const tdata<>&) { }

template<typename Head, typename... X, typename... Y>
void tdata_set(tdata<Head, X...>& lhs, tdata<Head, const Y...>& rhs) {
    lhs.head = rhs.head;
    tdata_set(lhs.tail(), rhs.tail());
}

template<size_t N, typename... Tn>
struct tdata_upcast_helper;

template<size_t N, typename Head, typename... Tail>
struct tdata_upcast_helper<N, Head, Tail...> {
    typedef typename tdata_upcast_helper<N-1, Tail...>::type type;
};

template<typename Head, typename... Tail>
struct tdata_upcast_helper<0, Head, Tail...> {
    typedef tdata<Head, Tail...> type;
};

template<typename T>
struct tdata_from_type_list;

template<typename... T>
struct tdata_from_type_list<util::type_list<T...>> {
    typedef tdata<T...> type;
};

template<typename... T>
inline void collect_tdata(tdata<T...>&) { }

template<typename Storage, typename... Args>
void collect_tdata(Storage& storage, const tdata<>&, const Args&... args) {
    collect_tdata(storage, args...);
}

template<typename Storage, typename Arg0, typename... Args>
void collect_tdata(Storage& storage, const Arg0& arg0, const Args&... args) {
    storage.head = arg0.head;
    collect_tdata(storage.tail(), arg0.tail(), args...);
}

} } // namespace cppa::detail

namespace cppa {

template<typename... Args>
detail::tdata<typename detail::implicit_conversions<typename util::rm_ref<Args>::type>::type...>
mk_tdata(Args&&... args) {
    return {std::forward<Args>(args)...};
}

template<size_t N, typename... Tn>
const typename util::at<N, Tn...>::type& get(const detail::tdata<Tn...>& tv) {
    static_assert(N < sizeof...(Tn), "N >= tv.size()");
    return static_cast<const typename detail::tdata_upcast_helper<N, Tn...>::type&>(tv).head;
}

template<size_t N, typename... Tn>
typename util::at<N, Tn...>::type& get_ref(detail::tdata<Tn...>& tv) {
    static_assert(N < sizeof...(Tn), "N >= tv.size()");
    return static_cast<typename detail::tdata_upcast_helper<N, Tn...>::type&>(tv).head;
}

} // namespace cppa

#endif // CPPA_TDATA_HPP
