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

#ifndef CPPA_DETAIL_DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
#define CPPA_DETAIL_DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP

#include <memory>

#include "cppa/unit.hpp"
#include "cppa/actor.hpp"
#include "cppa/anything.hpp"
#include "cppa/serializer.hpp"
#include "cppa/typed_actor.hpp"
#include "cppa/deserializer.hpp"

#include "cppa/detail/type_traits.hpp"
#include "cppa/detail/abstract_uniform_type_info.hpp"

#include "cppa/detail/types_array.hpp"

namespace cppa {
namespace detail {

using uniform_type_info_ptr = uniform_type_info_ptr;

// check if there's a 'push_back' that takes a C::value_type
template<typename T>
char sfinae_has_push_back(T* ptr, typename T::value_type* val = nullptr,
                          decltype(ptr->push_back(*val)) * = nullptr);

long sfinae_has_push_back(void*); // SFNINAE default

template<typename T>
struct is_stl_compliant_list {

    static constexpr bool value =
        detail::is_iterable<T>::value &&
        sizeof(sfinae_has_push_back(static_cast<T*>(nullptr))) == sizeof(char);

};

// check if there's an 'insert' that takes a C::value_type
template<typename T>
char sfinae_has_insert(T* ptr, typename T::value_type* val = nullptr,
                       decltype(ptr->insert(*val)) * = nullptr);

long sfinae_has_insert(void*); // SFNINAE default

template<typename T>
struct is_stl_compliant_map {

    static constexpr bool value =
        detail::is_iterable<T>::value &&
        sizeof(sfinae_has_insert(static_cast<T*>(nullptr))) == sizeof(char);

};

template<typename T>
struct is_stl_pair : std::false_type {};

template<typename F, typename S>
struct is_stl_pair<std::pair<F, S>> : std::true_type {};

using primitive_impl = std::integral_constant<int, 0>;
using list_impl = std::integral_constant<int, 1>;
using map_impl = std::integral_constant<int, 2>;
using pair_impl = std::integral_constant<int, 3>;
using recursive_impl = std::integral_constant<int, 9>;

template<typename T>
constexpr int impl_id() {
    return detail::is_primitive<T>::value ?
               0 :
               (is_stl_compliant_list<T>::value ?
                    1 :
                    (is_stl_compliant_map<T>::value ?
                         2 :
                         (is_stl_pair<T>::value ? 3 : 9)));
}

template<typename T>
struct deconst_pair {
    using type = T;

};

template<typename K, typename V>
struct deconst_pair<std::pair<K, V>> {
    using first_type = typename std::remove_const<K>::type;
    using second_type = typename std::remove_const<V>::type;
    using type = std::pair<first_type, second_type>;

};

class default_serialize_policy {

 public:

    template<typename T>
    void operator()(const T& val, serializer* s) const {
        std::integral_constant<int, impl_id<T>()> token;
        simpl(val, s, token);
    }

    template<typename T>
    void operator()(T& val, deserializer* d) const {
        std::integral_constant<int, impl_id<T>()> token;
        dimpl(val, d, token);
        // static_types_array<T>::arr[0]->deserialize(&val, d);
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
        using value_type = typename T::value_type;
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

    inline forwarding_serialize_policy(uniform_type_info_ptr uti)
            : m_uti(std::move(uti)) {}

    template<typename T>
    void operator()(const T& val, serializer* s) const {
        m_uti->serialize(&val, s);
    }

    template<typename T>
    void operator()(T& val, deserializer* d) const {
        m_uti->deserialize(&val, d);
    }

 private:

    uniform_type_info_ptr m_uti;

};

template<typename T, class AccessPolicy,
          class SerializePolicy = default_serialize_policy,
          bool IsEnum = std::is_enum<T>::value,
          bool IsEmptyType = std::is_class<T>::value&& std::is_empty<T>::value>
class member_tinfo : public detail::abstract_uniform_type_info<T> {

 public:

    member_tinfo(AccessPolicy apol, SerializePolicy spol)
            : m_apol(std::move(apol)), m_spol(std::move(spol)) {}

    member_tinfo(AccessPolicy apol) : m_apol(std::move(apol)) {}

    member_tinfo() {}

    void serialize(const void* vptr, serializer* s) const override {
        m_spol(m_apol(vptr), s);
    }

    void deserialize(void* vptr, deserializer* d) const override {
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

template<typename T, class A, class S>
class member_tinfo<T, A, S, false,
                   true> : public detail::abstract_uniform_type_info<T> {

 public:

    member_tinfo(const A&, const S&) {}

    member_tinfo(const A&) {}

    member_tinfo() {}

    void serialize(const void*, serializer*) const override {}

    void deserialize(void*, deserializer*) const override {}

};

template<typename T, class AccessPolicy, class SerializePolicy>
class member_tinfo<T, AccessPolicy, SerializePolicy, true,
                   false> : public detail::abstract_uniform_type_info<T> {

    using value_type = typename std::underlying_type<T>::type;

 public:

    member_tinfo(AccessPolicy apol, SerializePolicy spol)
            : m_apol(std::move(apol)), m_spol(std::move(spol)) {}

    member_tinfo(AccessPolicy apol) : m_apol(std::move(apol)) {}

    member_tinfo() {}

    void serialize(const void* p, serializer* s) const override {
        auto val = m_apol(p);
        m_spol(static_cast<value_type>(val), s);
    }

    void deserialize(void* p, deserializer* d) const override {
        value_type tmp;
        m_spol(tmp, d);
        m_apol(p, static_cast<T>(tmp));
    }

 private:

    AccessPolicy m_apol;
    SerializePolicy m_spol;

};

template<typename T, class C>
class memptr_access_policy {

 public:

    memptr_access_policy(const memptr_access_policy&) = default;
    memptr_access_policy& operator=(const memptr_access_policy&) = default;

    inline memptr_access_policy(T C::*memptr) : m_memptr(memptr) {}

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

    T C::*m_memptr;

};

template<class C, typename GRes, typename SRes, typename SArg>
class getter_setter_access_policy {

 public:

    using getter = GRes (C::*)() const;
    using setter = SRes (C::*)(SArg);

    getter_setter_access_policy(getter g, setter s) : m_get(g), m_set(s) {}

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

    template<typename U>
    inline void operator()(void* vptr, U&& value) const {
        *reinterpret_cast<T*>(vptr) = std::forward<U>(value);
    }

    static constexpr bool grants_mutable_access = true;

};

template<typename T, class C>
uniform_type_info_ptr new_member_tinfo(T C::*memptr) {
    using access_policy = memptr_access_policy<T, C>;
    using result_type = member_tinfo<T, access_policy>;
    return uniform_type_info_ptr(new result_type(memptr));
}

template<typename T, class C>
uniform_type_info_ptr new_member_tinfo(T C::*memptr,
                                       uniform_type_info_ptr meminf) {
    using access_policy = memptr_access_policy<T, C>;
    using tinfo = member_tinfo<T, access_policy, forwarding_serialize_policy>;
    return uniform_type_info_ptr(new tinfo(memptr, std::move(meminf)));
}

template<class C, typename GRes, typename SRes, typename SArg>
uniform_type_info_ptr new_member_tinfo(GRes (C::*getter)() const,
                                       SRes (C::*setter)(SArg)) {
    using access_policy = getter_setter_access_policy<C, GRes, SRes, SArg>;
    using value_type = typename detail::rm_const_and_ref<GRes>::type;
    using result_type = member_tinfo<value_type, access_policy>;
    return uniform_type_info_ptr(
        new result_type(access_policy(getter, setter)));
}

template<class C, typename GRes, typename SRes, typename SArg>
uniform_type_info_ptr new_member_tinfo(GRes (C::*getter)() const,
                                       SRes (C::*setter)(SArg),
                                       uniform_type_info_ptr meminf) {
    using access_policy = getter_setter_access_policy<C, GRes, SRes, SArg>;
    using value_type = typename detail::rm_const_and_ref<GRes>::type;
    using tinfo = member_tinfo<value_type, access_policy,
                               forwarding_serialize_policy>;
    return uniform_type_info_ptr(
        new tinfo(access_policy(getter, setter), std::move(meminf)));
}

template<typename T>
class default_uniform_type_info : public detail::abstract_uniform_type_info<T> {

 public:

    template<typename... Ts>
    default_uniform_type_info(Ts&&... args) {
        push_back(std::forward<Ts>(args)...);
    }

    default_uniform_type_info() {
        using result_type = member_tinfo<T, fake_access_policy<T>>;
        m_members.push_back(uniform_type_info_ptr(new result_type));
    }

    void serialize(const void* obj, serializer* s) const override {
        // serialize each member
        for (auto& m : m_members) m->serialize(obj, s);
    }

    void deserialize(void* obj, deserializer* d) const override {
        // deserialize each member
        for (auto& m : m_members) m->deserialize(obj, d);
    }

 protected:

    bool pod_mems_equals(const T& lhs, const T& rhs) const override {
        return pod_eq(lhs, rhs);
    }

 private:

    template<class C>
    typename std::enable_if<std::is_pod<C>::value, bool>::type
    pod_eq(const C& lhs, const C& rhs) const {
        for (auto& member : m_members) {
            if (!member->equals(&lhs, &rhs)) return false;
        }
        return true;
    }

    template<class C>
    typename std::enable_if<!std::is_pod<C>::value, bool>::type
    pod_eq(const C&, const C&) const {
        return false;
    }

    // terminates recursion
    inline void push_back() {}

    template<typename R, class C, typename... Ts>
    void push_back(R C::*memptr, Ts&&... args) {
        m_members.push_back(new_member_tinfo(memptr));
        push_back(std::forward<Ts>(args)...);
    }

    // pr.first = member pointer
    // pr.second = meta object to handle pr.first
    template<typename R, class C, typename... Ts>
    void push_back(
        const std::pair<R C::*, detail::abstract_uniform_type_info<R>*>& pr,
        Ts&&... args) {
        m_members.push_back(
            new_member_tinfo(pr.first, uniform_type_info_ptr(pr.second)));
        push_back(std::forward<Ts>(args)...);
    }

    // pr.first = getter / setter pair
    // pr.second = meta object to handle pr.first
    template<typename GR, typename SR, typename ST, typename C, typename... Ts>
    void push_back(const std::pair<GR (C::*)() const, // const-qualified getter
                                   SR (C::*)(ST) // setter with one argument
                                   >& pr,
                   Ts&&... args) {
        m_members.push_back(new_member_tinfo(pr.first, pr.second));
        push_back(std::forward<Ts>(args)...);
    }

    // pr.first = getter / setter pair
    // pr.second = meta object to handle pr.first
    template<typename GR, typename SR, typename ST, typename C, typename... Ts>
    void push_back(
        const std::pair<std::pair<GR (C::*)() const, // const-qualified getter
                                  SR (C::*)(ST)      // setter with one argument
                                  >,
                        detail::abstract_uniform_type_info<
                            typename detail::rm_const_and_ref<GR>::type>*>& pr,
        Ts&&... args) {
        m_members.push_back(new_member_tinfo(pr.first.first, pr.first.second,
                                             uniform_type_info_ptr(pr.second)));
        push_back(std::forward<Ts>(args)...);
    }

    std::vector<uniform_type_info_ptr> m_members;

};

template<typename... Rs>
class default_uniform_type_info<typed_actor<
    Rs...>> : public detail::abstract_uniform_type_info<typed_actor<Rs...>> {

 public:

    using handle_type = typed_actor<Rs...>;

    default_uniform_type_info() { sub_uti = uniform_typeid<actor>(); }

    void serialize(const void* obj, serializer* s) const override {
        auto tmp = actor_cast<actor>(deref(obj).address());
        sub_uti->serialize(&tmp, s);
    }

    void deserialize(void* obj, deserializer* d) const override {
        actor tmp;
        sub_uti->deserialize(&tmp, d);
        deref(obj) = actor_cast<handle_type>(tmp);
    }

 private:

    const uniform_type_info* sub_uti;

    static handle_type& deref(void* ptr) {
        return *reinterpret_cast<handle_type*>(ptr);
    }

    static const handle_type& deref(const void* ptr) {
        return *reinterpret_cast<const handle_type*>(ptr);
    }

};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
