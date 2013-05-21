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


#ifndef CPPA_DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
#define CPPA_DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP

#include <memory>

#include "cppa/anything.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/void_type.hpp"
#include "cppa/util/is_builtin.hpp"
#include "cppa/util/is_iterable.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/is_forward_iterator.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

#include "cppa/detail/types_array.hpp"
#include "cppa/detail/type_to_ptype.hpp"

namespace cppa { namespace detail {

typedef std::unique_ptr<uniform_type_info> unique_uti;

// check if there's a 'push_back' that takes a C::value_type
template<typename T>
char is_stl_compliant_list_fun(T* ptr,
                               typename T::value_type* val = nullptr,
                               decltype(ptr->push_back(*val))* = nullptr);

long is_stl_compliant_list_fun(void*); // SFNINAE default

template<typename T>
struct is_stl_compliant_list {

    static constexpr bool value = util::is_iterable<T>::value &&
        sizeof(is_stl_compliant_list_fun(static_cast<T*>(nullptr))) == sizeof(char);
};

// check if there's an 'insert' that takes a C::value_type
template<typename T>
char is_stl_compliant_map_fun(T* ptr,
                              typename T::value_type* val = nullptr,
                              decltype(ptr->insert(*val))* = nullptr);

long is_stl_compliant_map_fun(void*); // SFNINAE default

template<typename T>
struct is_stl_compliant_map {

    static constexpr bool value = util::is_iterable<T>::value &&
        sizeof(is_stl_compliant_map_fun(static_cast<T*>(nullptr))) == sizeof(char);

};

template<typename T>
struct is_stl_pair : std::false_type { };

template<typename F, typename S>
struct is_stl_pair<std::pair<F,S> > : std::true_type { };

template<typename T>
class builtin_member : public util::abstract_uniform_type_info<T> {

 public:

    builtin_member() : m_decorated(uniform_typeid<T>()) { }

    void serialize(const void* obj, serializer* s) const {
        m_decorated->serialize(obj, s);
    }

    void deserialize(void* obj, deserializer* d) const {
        m_decorated->deserialize(obj, d);
    }

 private:

    const uniform_type_info* m_decorated;

};

typedef std::integral_constant<int,0> primitive_impl;
typedef std::integral_constant<int,1> list_impl;
typedef std::integral_constant<int,2> map_impl;
typedef std::integral_constant<int,3> pair_impl;
typedef std::integral_constant<int,9> recursive_impl;

template<typename T>
constexpr int impl_id() {
    return util::is_primitive<T>::value
           ? 0
           : (is_stl_compliant_list<T>::value
              ? 1
              : (is_stl_compliant_map<T>::value
                 ? 2
                 : (is_stl_pair<T>::value
                    ? 3
                    : 9)));
}

template<typename T>
struct deconst_pair {
    typedef T type;
};

template<typename K, typename V>
struct deconst_pair<std::pair<K,V> > {
    typedef typename std::remove_const<K>::type first_type;
    typedef typename std::remove_const<V>::type second_type;
    typedef std::pair<first_type,second_type> type;
};

class default_serialize_policy {

 public:

    template<typename T>
    void operator()(const T& val, serializer* s) const {
        std::integral_constant<int,impl_id<T>()> token;
        simpl(val, s, token);
    }

    template<typename T>
    void operator()(T& val, deserializer* d) const {
        std::integral_constant<int,impl_id<T>()> token;
        dimpl(val, d, token);
        //static_types_array<T>::arr[0]->deserialize(&val, d);
    }

 private:

    template<typename T>
    void simpl(const T& val, serializer* s, primitive_impl) const {
        s->write_value(val);
    }

    template<typename T>
    void simpl(const T& val, serializer* s, list_impl) const {
        s->begin_sequence(val.size());
        for (auto i = val.begin(); i != val.end(); ++i) {
            (*this)(*i, s);
        }
        s->end_sequence();
    }

    template<typename T>
    void simpl(const T& val, serializer* s, map_impl) const {
        // lists and maps share code for serialization
        list_impl token;
        simpl(val, s, token);
    }

    template<typename T>
    void simpl(const T& val, serializer* s, pair_impl) const {
        (*this)(val.first, s);
        (*this)(val.second, s);
    }

    template<typename T>
    void simpl(const T& val, serializer* s, recursive_impl) const {
        static_types_array<T>::arr[0]->serialize(&val, s);
    }

    template<typename T>
    void dimpl(T& storage, deserializer* d, primitive_impl) const {
        storage = d->read<T>();
    }

    template<typename T>
    void dimpl(T& storage, deserializer* d, list_impl) const {
        typedef typename T::value_type value_type;
        storage.clear();
        size_t size = d->begin_sequence();
        for (size_t i = 0; i < size; ++i) {
            value_type tmp;
            (*this)(tmp, d);
            storage.push_back(std::move(tmp));
        }
        d->end_sequence();
    }

    template<typename T>
    void dimpl(T& storage, deserializer* d, map_impl) const {
        storage.clear();
        size_t size = d->begin_sequence();
        for (size_t i = 0; i < size; ++i) {
            typename deconst_pair<typename T::value_type>::type tmp;
            (*this)(tmp, d);
            storage.insert(tmp);
        }
        d->end_sequence();
    }

    template<typename T>
    void dimpl(T& storage, deserializer* d, pair_impl) const {
        (*this)(storage.first, d);
        (*this)(storage.second, d);
    }

    template<typename T>
    void dimpl(T& storage, deserializer* d, recursive_impl) const {
        static_types_array<T>::arr[0]->deserialize(&storage, d);
    }

};

class forwarding_serialize_policy {

 public:

    forwarding_serialize_policy(const forwarding_serialize_policy&) = delete;
    forwarding_serialize_policy& operator=(const forwarding_serialize_policy&) = delete;

    forwarding_serialize_policy(forwarding_serialize_policy&&) = default;
    forwarding_serialize_policy& operator=(forwarding_serialize_policy&&) = default;

    inline forwarding_serialize_policy(unique_uti uti)
    : m_uti(std::move(uti)) { }

    template<typename T>
    inline void operator()(const T& val, serializer* s) const {
        m_uti->serialize(&val, s);
    }

    template<typename T>
    inline void operator()(T& val, deserializer* d) const {
        m_uti->deserialize(&val, d);
    }

 private:

    unique_uti m_uti;

};

template<typename T, class AccessPolicy, class SerializePolicy = default_serialize_policy>
class member_tinfo : public util::abstract_uniform_type_info<T> {

 public:

    member_tinfo(AccessPolicy apol, SerializePolicy spol)
    : m_apol(std::move(apol)), m_spol(std::move(spol)) { }

    member_tinfo(AccessPolicy apol) : m_apol(std::move(apol)) { }

    member_tinfo() { }

    void serialize(const void* vptr, serializer* s) const {
        m_spol(m_apol(vptr), s);
    }

    void deserialize(void* vptr, deserializer* d) const {
        std::integral_constant<bool, AccessPolicy::grants_mutable_access> token;
        ds(vptr, d, token);
    }

 private:

    void ds(void* p, deserializer* d, std::true_type) const {
        m_spol(m_apol(p), d);
    }

    void ds(void* p, deserializer* d, std::false_type) const {
        T tmp;
        m_spol(tmp, d);
        m_apol(p, std::move(tmp));
    }

    AccessPolicy m_apol;
    SerializePolicy m_spol;

};

template<typename T, class C>
class memptr_access_policy {

 public:

    memptr_access_policy(const memptr_access_policy&) = default;
    memptr_access_policy& operator=(const memptr_access_policy&) = default;

    inline memptr_access_policy(T C::* memptr) : m_memptr(memptr) { }

    inline T& operator()(void* vptr) const {
        auto ptr = reinterpret_cast<C*>(vptr);
        return *ptr.*m_memptr;
    }

    inline const T& operator()(const void* vptr) const {
        auto ptr = reinterpret_cast<const C*>(vptr);
        return *ptr.*m_memptr;
    }

    static constexpr bool grants_mutable_access = true;

 private:

    T C::* m_memptr;

};

template<class C, typename GRes, typename SRes, typename SArg>
class getter_setter_access_policy {

 public:

    typedef GRes (C::*getter)() const;
    typedef SRes (C::*setter)(SArg);

    getter_setter_access_policy(const getter_setter_access_policy&) = default;
    getter_setter_access_policy& operator=(const getter_setter_access_policy&) = default;

    getter_setter_access_policy(getter g, setter s) : m_get(g), m_set(s) { }

    inline GRes operator()(const void* vptr) const {
        auto ptr = reinterpret_cast<const C*>(vptr);
        return (*ptr.*m_get)();
    }

    template<typename Arg>
    inline void operator()(void* vptr, Arg&& value) const {
        auto ptr = reinterpret_cast<C*>(vptr);
        return (*ptr.*m_set)(std::forward<Arg>(value));
    }

    static constexpr bool grants_mutable_access = false;

 private:

    getter m_get;
    setter m_set;

};

template<typename T>
struct fake_access_policy {

    inline T& operator()(void* vptr) const {
        return *reinterpret_cast<T*>(vptr);
    }

    inline const T& operator()(const void* vptr) const {
        return *reinterpret_cast<const T*>(vptr);
    }

    static constexpr bool grants_mutable_access = true;

};

template<typename T, class C>
unique_uti new_member_tinfo(T C::* memptr) {
    typedef memptr_access_policy<T,C> access_policy;
    typedef member_tinfo<T,access_policy> result_type;
    return unique_uti(new result_type(memptr));
}

template<typename T, class C>
unique_uti new_member_tinfo(T C::* memptr, std::unique_ptr<uniform_type_info> meminf) {
    typedef memptr_access_policy<T,C> access_policy;
    typedef member_tinfo<T,access_policy,forwarding_serialize_policy> result_type;
    return unique_uti(new result_type(memptr, std::move(meminf)));
}

template<class C, typename GRes, typename SRes, typename SArg>
unique_uti new_member_tinfo(GRes (C::*getter)() const, SRes (C::*setter)(SArg)) {
    typedef getter_setter_access_policy<C, GRes, SRes, SArg> access_policy;
    typedef typename util::rm_ref<GRes>::type value_type;
    typedef member_tinfo<value_type,access_policy> result_type;
    return unique_uti(new result_type(access_policy(getter, setter)));
}

template<class C, typename GRes, typename SRes, typename SArg>
unique_uti new_member_tinfo(GRes (C::*getter)() const, SRes (C::*setter)(SArg), std::unique_ptr<uniform_type_info> meminf) {
    typedef getter_setter_access_policy<C, GRes, SRes, SArg> access_policy;
    typedef typename util::rm_ref<GRes>::type value_type;
    typedef member_tinfo<value_type,access_policy,forwarding_serialize_policy> result_type;
    return unique_uti(new result_type(access_policy(getter, setter), std::move(meminf)));
}

template<typename T>
class default_uniform_type_info_impl : public util::abstract_uniform_type_info<T> {

    std::vector<unique_uti> m_members;

    // terminates recursion
    inline void push_back() { }

    template<typename R, class C, typename... Ts>
    void push_back(R C::* memptr, Ts&&... args) {
        m_members.push_back(new_member_tinfo(memptr));
        push_back(std::forward<Ts>(args)...);
    }

    // pr.first = member pointer
    // pr.second = meta object to handle pr.first
    template<typename R, class C, typename... Ts>
    void push_back(const std::pair<R C::*, util::abstract_uniform_type_info<R>*>& pr,
                   Ts&&... args) {
        m_members.push_back(new_member_tinfo(pr.first, unique_uti(pr.second)));
        push_back(std::forward<Ts>(args)...);
    }

    // pr.first = getter / setter pair
    // pr.second = meta object to handle pr.first
    template<typename GRes, typename SRes, typename SArg, typename C, typename... Ts>
    void push_back(const std::pair<GRes (C::*)() const, SRes (C::*)(SArg)>& pr,
                   Ts&&... args) {
        m_members.push_back(new_member_tinfo(pr.first, pr.second));
        push_back(std::forward<Ts>(args)...);
    }

    // pr.first = getter / setter pair
    // pr.second = meta object to handle pr.first
    template<typename GRes, typename SRes, typename SArg, typename C, typename... Ts>
    void push_back(const std::pair<std::pair<GRes (C::*)() const, SRes (C::*)(SArg)>, util::abstract_uniform_type_info<typename util::rm_ref<GRes>::type>*>& pr,
                   Ts&&... args) {
        m_members.push_back(new_member_tinfo(pr.first.first, pr.first.second, unique_uti(pr.second)));
        push_back(std::forward<Ts>(args)...);
    }

 public:

    template<typename... Ts>
    default_uniform_type_info_impl(Ts&&... args) {
        push_back(std::forward<Ts>(args)...);
    }

    default_uniform_type_info_impl() {
        typedef member_tinfo<T,fake_access_policy<T> > result_type;
        m_members.push_back(unique_uti(new result_type));
    }

    void serialize(const void* obj, serializer* s) const {
        s->begin_object(this->name());
        for (auto& m : m_members) {
            m->serialize(obj, s);
        }
        s->end_object();
    }

    void deserialize(void* obj, deserializer* d) const {
        this->assert_type_name(d);
        d->begin_object(this->name());
        for (auto& m : m_members) {
            m->deserialize(obj, d);
        }
        d->end_object();
    }

};

} } // namespace detail

#endif // CPPA_DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
