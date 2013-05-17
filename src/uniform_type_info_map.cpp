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


#include <array>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

#include "cppa/any_tuple.hpp"
#include "cppa/message_header.hpp"
#include "cppa/actor_addressing.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/limited_vector.hpp"

#include "cppa/detail/uniform_type_info_map.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

namespace cppa { namespace detail {

// maps demangled names to libcppa names
// WARNING: this map is sorted, insert new elements *in sorted order* as well!
/* extern */ const char* mapped_type_names[][2] = {
    { "bool",                                                                           "bool" },
    { "cppa::any_tuple",                                                                "@tuple" },
    { "cppa::atom_value",                                                               "@atom" },
    { "cppa::intrusive_ptr<cppa::actor>",                                               "@actor" },
    { "cppa::intrusive_ptr<cppa::channel>",                                             "@channel" },
    { "cppa::intrusive_ptr<cppa::group>",                                               "@group" },
    { "cppa::intrusive_ptr<cppa::process_information>",                                 "@proc"},
    { "cppa::message_header",                                                           "@header" },
    { "cppa::nullptr_t",                                                                "@null" },
    { "cppa::util::duration",                                                           "@duration" },
    { "cppa::util::void_type",                                                          "@0" },
    { "double",                                                                         "double" },
    { "float",                                                                          "float" },
    { "long double",                                                                    "@ldouble" },
    { "std::basic_string<@i16,std::char_traits<@i16>,std::allocator<@i16>>",            "@u16str" },
    { "std::basic_string<@i32,std::char_traits<@i32>,std::allocator<@i32>>",            "@u32str" },
    { "std::basic_string<@i8,std::char_traits<@i8>,std::allocator<@i8>>",               "@str" },
    { "std::basic_string<@u16,std::char_traits<@u16>,std::allocator<@u16>>",            "@u16str" },
    { "std::basic_string<@u32,std::char_traits<@u32>,std::allocator<@u32>>",            "@u32str" },
    { "std::map<@str,@str,std::less<@str>,std::allocator<std::pair<const @str,@str>>>", "@strmap" }
};

// maps sizeof(T) => {unsigned name, signed name}
/* extern */ const char* mapped_int_names[][2] = {
    {nullptr, nullptr}, // no int type with 0 bytes
    {"@u8",  "@i8"},
    {"@u16", "@i16"},
    {nullptr, nullptr}, // no int type with 3 bytes
    {"@u32", "@i32"},
    {nullptr, nullptr}, // no int type with 5 bytes
    {nullptr, nullptr}, // no int type with 6 bytes
    {nullptr, nullptr}, // no int type with 7 bytes
    {"@u64", "@i64"}
};

const char* mapped_name_by_decorated_name(const char* cstr) {
    using namespace std;
    auto cmp = [](const char** lhs, const char* rhs) {
        return strcmp(lhs[0], rhs) < 0;
    };
    auto e = end(mapped_type_names);
    auto i = lower_bound(begin(mapped_type_names), e, cstr, cmp);
    if (i != e && strcmp(cstr, (*i)[0]) == 0) return (*i)[1];
#   if defined(__GNUC__) && !defined(__clang__)
    // for some reason, GCC returns "std::string" as RTTI type name
    // instead of std::basic_string<...>
    if (strcmp("std::string", cstr) == 0) return mapped_name<std::string>();
#   endif
    return cstr;
}


namespace {

inline bool operator==(const util::void_type&, const util::void_type&) {
    return true;
}

template<typename T> struct type_token { };

void serialize_nullptr(serializer* sink) {
    sink->begin_object(mapped_name<std::nullptr_t>());
    sink->end_object();
}

void deserialize_nullptr(deserializer* source) {
    source->begin_object(mapped_name<std::nullptr_t>());
    source->end_object();
}

void assert_type_name(const char* expected_name, deserializer* source) {
    auto tname = source->seek_object();
    if (tname != expected_name) {
        std::string error_msg = "wrong type name found; expected \"";
        error_msg += expected_name;
        error_msg += "\", found \"";
        error_msg += tname;
        error_msg += "\"";
        throw std::logic_error(std::move(error_msg));
    }
}

template<typename T>
void serialize_impl(const T& val, serializer* sink) {
    sink->begin_object(mapped_name<T>());
    sink->write_value(val);
    sink->end_object();
}

template<typename T>
void deserialize_impl(T& val, deserializer* source) {
    assert_type_name(mapped_name<T>(), source);
    source->begin_object(mapped_name<T>());
    val = source->read<T>();
    source->end_object();
}

void serialize_impl(const util::void_type&, serializer* sink) {
    sink->begin_object(mapped_name<util::void_type>());
    sink->end_object();
}

void deserialize_impl(util::void_type&, deserializer* source) {
    auto tname = mapped_name<util::void_type>();
    assert_type_name(tname, source);
    source->begin_object(tname);
    source->end_object();
}

void serialize_impl(const actor_ptr& ptr, serializer* sink) {
    auto impl = sink->addressing();
    if (impl) impl->write(sink, ptr);
    else throw std::runtime_error("unable to serialize actor_ptr: "
                                  "no actor addressing defined");
}

void deserialize_impl(actor_ptr& ptrref, deserializer* source) {
    auto impl = source->addressing();
    if (impl) ptrref = impl->read(source);
    else throw std::runtime_error("unable to deserialize actor_ptr: "
                                  "no actor addressing defined");
}

void serialize_impl(const group_ptr& ptr, serializer* sink) {
    if (ptr == nullptr) serialize_nullptr(sink);
    else {
        sink->begin_object(mapped_name<group_ptr>());
        sink->write_value(ptr->module_name());
        ptr->serialize(sink);
        sink->end_object();
    }
}

void deserialize_impl(group_ptr& ptrref, deserializer* source) {
    auto tname = mapped_name<group_ptr>();
    auto cname = source->seek_object();
    if (cname != tname) {
        if (cname == mapped_name<std::nullptr_t>()) {
            deserialize_nullptr(source);
            ptrref.reset();
        }
        else assert_type_name(tname, source); // throws
    }
    else {
        source->begin_object(tname);
        auto modname = source->read<std::string>();
        ptrref = group::get_module(modname)
                ->deserialize(source);
        source->end_object();
    }
}

void serialize_impl(const channel_ptr& ptr, serializer* sink) {
    auto tname = mapped_name<channel_ptr>();
    sink->begin_object(tname);
    if (ptr == nullptr) serialize_nullptr(sink);
    else {
        auto aptr = ptr.downcast<actor>();
        if (aptr) serialize_impl(aptr, sink);
        else {
            auto gptr = ptr.downcast<group>();
            if (gptr) serialize_impl(gptr, sink);
            else throw std::logic_error("channel is neither "
                                        "an actor nor a group");
        }
    }
    sink->end_object();
}

void deserialize_impl(channel_ptr& ptrref, deserializer* source) {
    auto tname = mapped_name<channel_ptr>();
    assert_type_name(tname, source);
    source->begin_object(tname);
    auto subobj = source->peek_object();
    if (subobj == mapped_name<actor_ptr>()) {
        actor_ptr tmp;
        deserialize_impl(tmp, source);
        ptrref = tmp;
    }
    else if (subobj == mapped_name<group_ptr>()) {
        group_ptr tmp;
        deserialize_impl(tmp, source);
        ptrref = tmp;
    }
    else if (subobj == mapped_name<std::nullptr_t>()) {
        static_cast<void>(source->seek_object());
        deserialize_nullptr(source);
        ptrref.reset();
    }
    else throw std::logic_error("unexpected type name: " + subobj);
    source->end_object();
}

void serialize_impl(const any_tuple& tup, serializer* sink) {
    auto tname = mapped_name<any_tuple>();
    sink->begin_object(tname);
    sink->begin_sequence(tup.size());
    for (size_t i = 0; i < tup.size(); ++i) {
        tup.type_at(i)->serialize(tup.at(i), sink);
    }
    sink->end_sequence();
    sink->end_object();
}

void deserialize_impl(any_tuple& atref, deserializer* source) {
    auto tname = mapped_name<any_tuple>();
    assert_type_name(tname, source);
    auto result = new detail::object_array;
    source->begin_object(tname);
    size_t tuple_size = source->begin_sequence();
    for (size_t i = 0; i < tuple_size; ++i) {
        auto uname = source->peek_object();
        auto utype = uniform_type_info::from(uname);
        result->push_back(utype->deserialize(source));
    }
    source->end_sequence();
    source->end_object();
    atref = any_tuple{result};
}

void serialize_impl(const message_header& hdr, serializer* sink) {
    auto tname = mapped_name<message_header>();
    sink->begin_object(tname);
    serialize_impl(hdr.sender, sink);
    serialize_impl(hdr.receiver, sink);
    sink->write_value(hdr.id.integer_value());
    sink->end_object();
}

void deserialize_impl(message_header& hdr, deserializer* source) {
    auto tname = mapped_name<message_header>();
    assert_type_name(tname, source);
    source->begin_object(tname);
    deserialize_impl(hdr.sender, source);
    deserialize_impl(hdr.receiver, source);
    auto msg_id = source->read<std::uint64_t>();
    hdr.id = message_id::from_integer_value(msg_id);
    source->end_object();
}

void serialize_impl(const process_information_ptr& ptr, serializer* sink) {
    if (ptr == nullptr) serialize_nullptr(sink);
    else {
        auto tname = mapped_name<process_information_ptr>();
        sink->begin_object(tname);
        sink->write_value(ptr->process_id());
        sink->write_raw(ptr->node_id().size(), ptr->node_id().data());
        sink->end_object();
    }
}

void deserialize_impl(process_information_ptr& ptr, deserializer* source) {
    auto tname = mapped_name<process_information_ptr>();
    auto cname = source->seek_object();
    if (cname != tname) {
        if (cname == mapped_name<std::nullptr_t>()) {
            deserialize_nullptr(source);
            ptr.reset();
        }
        else assert_type_name(tname, source); // throws
    }
    else {
        source->begin_object(tname);
        auto id = source->read<uint32_t>();
        process_information::node_id_type nid;
        source->read_raw(nid.size(), nid.data());
        source->end_object();
        ptr.reset(new process_information{id, nid});
    }
}

void serialize_impl(const atom_value& val, serializer* sink) {
    sink->begin_object(mapped_name<atom_value>());
    sink->write_value(static_cast<uint64_t>(val));
    sink->end_object();
}

void deserialize_impl(atom_value& val, deserializer* source) {
    auto tname = mapped_name<atom_value>();
    assert_type_name(tname, source);
    source->begin_object(tname);
    val = static_cast<atom_value>(source->read<uint64_t>());
    source->end_object();
}

void serialize_impl(const util::duration& val, serializer* sink) {
    auto tname = mapped_name<util::duration>();
    sink->begin_object(tname);
    sink->write_value(static_cast<uint32_t>(val.unit));
    sink->write_value(val.count);
    sink->end_object();
}

void deserialize_impl(util::duration& val, deserializer* source) {
    auto tname = mapped_name<util::duration>();
    assert_type_name(tname, source);
    source->begin_object(tname);
    auto unit_val = source->read<uint32_t>();
    auto count_val = source->read<uint32_t>();
    source->end_object();
    switch (unit_val) {
        case 1:
            val.unit = util::time_unit::seconds;
            break;

        case 1000:
            val.unit = util::time_unit::milliseconds;
            break;

        case 1000000:
            val.unit = util::time_unit::microseconds;
            break;

        default:
            val.unit = util::time_unit::none;
            break;
    }
    val.count = count_val;
}

void serialize_impl(bool val, serializer* sink) {
    sink->begin_object(mapped_name<bool>());
    sink->write_value(static_cast<uint8_t>(val ? 1 : 0));
    sink->end_object();
}

void deserialize_impl(bool& val, deserializer* source) {
    auto tname = mapped_name<bool>();
    assert_type_name(tname, source);
    source->begin_object(tname);
    val = source->read<uint8_t>() != 0;
    source->end_object();
}

inline bool types_equal(const std::type_info* lhs, const std::type_info* rhs) {
    // in some cases (when dealing with dynamic libraries), address can be
    // different although types are equal
    return lhs == rhs || *lhs == *rhs;
}

template<typename T>
class uti_base : public uniform_type_info {

 protected:

    uti_base() : m_native(&typeid(T)) { }

    bool equals(const std::type_info& ti) const {
        return types_equal(m_native, &ti);
    }

    bool equals(const void* lhs, const void* rhs) const {
        return deref(lhs) == deref(rhs);
    }

    void* new_instance(const void* ptr) const {
        return (ptr) ? new T(deref(ptr)) : new T;
    }

    void delete_instance(void* instance) const {
        delete reinterpret_cast<T*>(instance);
    }

    static inline const T& deref(const void* ptr) {
        return *reinterpret_cast<const T*>(ptr);
    }

    static inline T& deref(void* ptr) {
        return *reinterpret_cast<T*>(ptr);
    }

    const std::type_info* m_native;

};

template<typename T>
class uti_impl : public uti_base<T> {

    typedef uti_base<T> super;

 public:

    const char* name() const {
        return mapped_name<T>();
    }

 protected:

    void serialize(const void *instance, serializer *sink) const {
        serialize_impl(super::deref(instance), sink);
    }

    void deserialize(void* instance, deserializer* source) const {
        deserialize_impl(super::deref(instance), source);
    }

};

class abstract_int_tinfo : public uniform_type_info {

 public:

    void add_native_type(const std::type_info& ti) {
        // only push back if not already set
        auto predicate = [&](const std::type_info* ptr) { return ptr == &ti; };
        if (std::none_of(m_natives.begin(), m_natives.end(), predicate))
            m_natives.push_back(&ti);
    }

 protected:

    std::vector<const std::type_info*> m_natives;

};

// unfortunately, one integer type can be mapped to multiple types
template<typename T>
class int_tinfo : public abstract_int_tinfo {

 public:

    void serialize(const void* instance, serializer* sink) const {
        auto& val = deref(instance);
        sink->begin_object(static_name());
        sink->write_value(val);
        sink->end_object();
    }

    void deserialize(void* instance, deserializer* source) const {
        assert_type_name(static_name(), source);
        auto& ref = deref(instance);
        source->begin_object(static_name());
        ref = source->read<T>();
        source->end_object();
    }

    const char* name() const {
        return static_name();
    }

 protected:

    bool equals(const std::type_info& ti) const {
        auto tptr = &ti;
        return std::any_of(m_natives.begin(), m_natives.end(), [tptr](const std::type_info* ptr) {
            return types_equal(ptr, tptr);
        });
    }

    bool equals(const void* lhs, const void* rhs) const {
        return deref(lhs) == deref(rhs);
    }

    void* new_instance(const void* ptr) const {
        return (ptr) ? new T(deref(ptr)) : new T;
    }

    void delete_instance(void* instance) const {
        delete reinterpret_cast<T*>(instance);
    }

 private:

    inline static const T& deref(const void* ptr) {
        return *reinterpret_cast<const T*>(ptr);
    }

    inline static T& deref(void* ptr) {
        return *reinterpret_cast<T*>(ptr);
    }

    static inline const char* static_name() {
        return mapped_int_name<T>();
    }

};

template<typename T>
void push_native_type(abstract_int_tinfo* m [][2]) {
    m[sizeof(T)][std::is_signed<T>::value ? 1 : 0]->add_native_type(typeid(T));
}

template<typename T0, typename T1, typename... Ts>
void push_native_type(abstract_int_tinfo* m [][2]) {
    push_native_type<T0>(m);
    push_native_type<T1,Ts...>(m);
}

class utim_impl : public uniform_type_info_map {

 public:

    void initialize() {
        // maps sizeof(integer_type) to {signed_type, unsigned_type}
        abstract_int_tinfo* mapping[][2] = {
            {nullptr, nullptr},         // no integer type for sizeof(T) == 0
            {&m_type_u8,  &m_type_i8},
            {&m_type_u16, &m_type_i16},
            {nullptr, nullptr},         // no integer type for sizeof(T) == 3
            {&m_type_u32, &m_type_i32},
            {nullptr, nullptr},         // no integer type for sizeof(T) == 5
            {nullptr, nullptr},         // no integer type for sizeof(T) == 6
            {nullptr, nullptr},         // no integer type for sizeof(T) == 7
            {&m_type_u64, &m_type_i64}
        };
        push_native_type<char,                  signed char,
                         unsigned char,         short,
                         signed short,          unsigned short,
                         short int,             signed short int,
                         unsigned short int,    int,
                         signed int,            unsigned int,
                         long int,              signed long int,
                         unsigned long int,     long,
                         signed long,           unsigned long,
                         long long,             signed long long,
                         unsigned long long,    wchar_t,
                         std::int8_t,           std::uint8_t,
                         std::int16_t,          std::uint16_t,
                         std::int32_t,          std::uint32_t,
                         std::int64_t,          std::uint64_t,
                         char16_t,              char32_t,
                         size_t,                ptrdiff_t,
                         intptr_t                                >(mapping);
        // fill builtin types *in sorted order* (by uniform name)
        size_t i = 0;
        m_builtin_types[i++] = &m_type_void;
        m_builtin_types[i++] = &m_type_actor;
        m_builtin_types[i++] = &m_type_atom;
        m_builtin_types[i++] = &m_type_channel;
        m_builtin_types[i++] = &m_type_duration;
        m_builtin_types[i++] = &m_type_group;
        m_builtin_types[i++] = &m_type_header;
        m_builtin_types[i++] = &m_type_i16;
        m_builtin_types[i++] = &m_type_i32;
        m_builtin_types[i++] = &m_type_i64;
        m_builtin_types[i++] = &m_type_i8;
        m_builtin_types[i++] = &m_type_long_double;
        m_builtin_types[i++] = &m_type_proc;
        m_builtin_types[i++] = &m_type_str;
        m_builtin_types[i++] = &m_type_strmap;
        m_builtin_types[i++] = &m_type_tuple;
        m_builtin_types[i++] = &m_type_u16;
        m_builtin_types[i++] = &m_type_u16str;
        m_builtin_types[i++] = &m_type_u32;
        m_builtin_types[i++] = &m_type_u32str;
        m_builtin_types[i++] = &m_type_u64;
        m_builtin_types[i++] = &m_type_u8;
        m_builtin_types[i++] = &m_type_bool;
        m_builtin_types[i++] = &m_type_double;
        m_builtin_types[i++] = &m_type_float;
        CPPA_REQUIRE(i == m_builtin_types.size());
#       if CPPA_DEBUG_MODE
        auto cmp = [](pointer lhs, pointer rhs) {
            return strcmp(lhs->name(), rhs->name()) < 0;
        };
        if (!std::is_sorted(m_builtin_types.begin(), m_builtin_types.end(), cmp)) {
            std::cerr << "FATAL: uniform type map not sorted" << std::endl;

            std::cerr << "order is:" << std::endl;
            for (auto ptr : m_builtin_types) std::cerr << ptr->name() << std::endl;

            std::sort(m_builtin_types.begin(), m_builtin_types.end(), cmp);
            std::cerr << "\norder should be:" << std::endl;
            for (auto ptr : m_builtin_types) std::cerr << ptr->name() << std::endl;
            abort();
        }
        auto cmp2 = [](const char** lhs, const char** rhs) {
            return strcmp(lhs[0], rhs[0]) < 0;
        };
        if (!std::is_sorted(std::begin(mapped_type_names), std::end(mapped_type_names), cmp2)) {
            std::cerr << "FATAL: mapped_type_names not sorted" << std::endl;
            abort();
        }
#       endif
    }

    pointer by_rtti(const std::type_info& ti) const {
        auto res = find_rtti(m_builtin_types, ti);
        return (res) ? res : find_rtti(m_user_types, ti);
    }

    pointer by_uniform_name(const std::string& name) const {
        auto res = find_name(m_builtin_types, name);
        return (res) ? res : find_name(m_user_types, name);
    }

    std::vector<pointer> get_all() const {
        std::vector<pointer> res;
        res.reserve(m_builtin_types.size() + m_user_types.size());
        res.insert(res.end(), m_builtin_types.begin(), m_builtin_types.end());
        res.insert(res.end(), m_user_types.begin(), m_user_types.end());
        return std::move(res);
    }

    bool insert(uniform_type_info* uti) {
        auto e = m_user_types.end();
        auto i = std::lower_bound(m_user_types.begin(), e, uti, [](uniform_type_info* lhs, pointer rhs) {
            return strcmp(lhs->name(), rhs->name()) < 0;
        });
        if (i == e) m_user_types.push_back(uti);
        else {
            if (strcmp(uti->name(), (*i)->name()) == 0) {
                // type already known
                return false;
            }
            // insert after lower bound (vector is always sorted)
            m_user_types.insert(i, uti);
        }
        return true;
    }

    ~utim_impl() {
        for (auto ptr : m_user_types) delete ptr;
        m_user_types.clear();
    }

 private:

    typedef std::map<std::string,std::string> strmap;

    uti_impl<process_information_ptr>       m_type_proc;
    uti_impl<channel_ptr>                   m_type_channel;
    uti_impl<actor_ptr>                     m_type_actor;
    uti_impl<group_ptr>                     m_type_group;
    uti_impl<any_tuple>                     m_type_tuple;
    uti_impl<util::duration>                m_type_duration;
    uti_impl<message_header>                m_type_header;
    uti_impl<util::void_type>               m_type_void;
    uti_impl<atom_value>                    m_type_atom;
    uti_impl<std::string>                   m_type_str;
    uti_impl<std::u16string>                m_type_u16str;
    uti_impl<std::u32string>                m_type_u32str;
    default_uniform_type_info_impl<strmap>  m_type_strmap;
    uti_impl<bool>                          m_type_bool;
    uti_impl<float>                         m_type_float;
    uti_impl<double>                        m_type_double;
    uti_impl<long double>                   m_type_long_double;
    int_tinfo<std::int8_t>                  m_type_i8;
    int_tinfo<std::uint8_t>                 m_type_u8;
    int_tinfo<std::int16_t>                 m_type_i16;
    int_tinfo<std::uint16_t>                m_type_u16;
    int_tinfo<std::int32_t>                 m_type_i32;
    int_tinfo<std::uint32_t>                m_type_u32;
    int_tinfo<std::int64_t>                 m_type_i64;
    int_tinfo<std::uint64_t>                m_type_u64;

    // both containers are sorted by uniform name
    std::array<pointer,25> m_builtin_types;
    std::vector<uniform_type_info*> m_user_types;

    template<typename Container>
    pointer find_rtti(const Container& c, const std::type_info& ti) const {
        auto e = c.end();
        auto i = std::find_if(c.begin(), e, [&](pointer p) {
            return p->equals(ti);
        });
        return (i == e) ? nullptr : *i;
    }

    template<typename Container>
    pointer find_name(const Container& c, const std::string& name) const {
        auto e = c.end();
        // both containers are sorted
        auto i = std::lower_bound(c.begin(), e, name, [](pointer p, const std::string& n) {
            return p->name() < n;
        });
        return (i != e && (*i)->name() == name) ? *i : nullptr;
    }

};

} // namespace <anonymous>

uniform_type_info_map* uniform_type_info_map::create_singleton() {
    return new utim_impl;
}

uniform_type_info_map::~uniform_type_info_map() { }

} } // namespace cppa::detail
