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
#include <type_traits>

#include "cppa/logging.hpp"
#include "cppa/announce.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/message_header.hpp"
#include "cppa/actor_addressing.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/algorithm.hpp"
#include "cppa/util/scope_guard.hpp"
#include "cppa/util/limited_vector.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

#include "cppa/detail/object_array.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

namespace cppa { namespace detail {

// maps demangled names to libcppa names
// WARNING: this map is sorted, insert new elements *in sorted order* as well!
/* extern */ const char* mapped_type_names[][2] = {
    { "bool",                                           "bool" },
    { "cppa::any_tuple",                                "@tuple" },
    { "cppa::atom_value",                               "@atom" },
    { "cppa::intrusive_ptr<cppa::actor>",               "@actor" },
    { "cppa::intrusive_ptr<cppa::channel>",             "@channel" },
    { "cppa::intrusive_ptr<cppa::group>",               "@group" },
    { "cppa::intrusive_ptr<cppa::process_information>", "@proc"},
    { "cppa::io::accept_handle",                        "@ac_hdl" },
    { "cppa::io::connection_handle",                    "@cn_hdl" },
    { "cppa::message_header",                           "@header" },
    { "cppa::nullptr_t",                                "@null" },
    { "cppa::unit_t",                                   "@0" },
    { "cppa::util::buffer",                             "@buffer" },
    { "cppa::util::duration",                           "@duration" },
    { "double",                                         "double" },
    { "float",                                          "float" },
    { "long double",                                    "@ldouble" },
    // std::u16string
    { "std::basic_string<@i16,std::char_traits<@i16>,std::allocator<@i16>>",
      "@u16str" },
    // std::u32string
    { "std::basic_string<@i32,std::char_traits<@i32>,std::allocator<@i32>>",
      "@u32str" },
    // std::string
    { "std::basic_string<@i8,std::char_traits<@i8>,std::allocator<@i8>>",
      "@str" },
    // std::u16string (again, using unsigned char type)
    { "std::basic_string<@u16,std::char_traits<@u16>,std::allocator<@u16>>",
      "@u16str" },
    // std::u32string (again, using unsigned char type)
    { "std::basic_string<@u32,std::char_traits<@u32>,std::allocator<@u32>>",
      "@u32str" },
    // std::map<std::string,std::string>
    { "std::map<@str,@str,std::less<@str>,std::allocator<std::pair<const @str,@str>>>",
      "@strmap" }
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

std::string mapped_name_by_decorated_name(std::string&& str) {
    auto res = mapped_name_by_decorated_name(str.c_str());
    if (res == str.c_str()) return std::move(str);
    return res;
}

namespace {

inline bool operator==(const unit_t&, const unit_t&) {
    return true;
}

template<typename T> struct type_token { };

template<typename T>
inline typename std::enable_if<util::is_primitive<T>::value>::type
serialize_impl(const T& val, serializer* sink) {
    sink->write_value(val);
}

template<typename T>
inline typename std::enable_if<util::is_primitive<T>::value>::type
deserialize_impl(T& val, deserializer* source) {
    val = source->read<T>();
}

template<typename T>
inline void serialize_impl(const detail::handle<T>& hdl, serializer* sink) {
    sink->write_value(static_cast<int32_t>(hdl.id()));
}

template<typename T>
inline void deserialize_impl(detail::handle<T>& hdl, deserializer* source) {
    hdl = T::from_int(source->read<int32_t>());
}

inline void serialize_impl(const unit_t&, serializer*) { }

inline void deserialize_impl(unit_t&, deserializer*) { }

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
    if (ptr == nullptr) {
        // write an empty string as module name
        std::string empty_string;
        sink->write_value(empty_string);
    }
    else {
        sink->write_value(ptr->module_name());
        ptr->serialize(sink);
    }
}

void deserialize_impl(group_ptr& ptrref, deserializer* source) {
    auto modname = source->read<std::string>();
    if (modname.empty()) ptrref.reset();
    else ptrref = group::get_module(modname)->deserialize(source);
}

void serialize_impl(const channel_ptr& ptr, serializer* sink) {
    // channel is an abstract base class that's either an actor or a group
    // to indicate that, we write a flag first, that is
    //     0 if ptr == nullptr
    //     1 if ptr points to an actor
    //     2 if ptr points to a group
    uint8_t flag = 0;
    auto wr_nullptr = [&] {
        sink->write_value(flag);
    };
    if (ptr == nullptr) wr_nullptr();
    else {
        auto aptr = ptr.downcast<actor>();
        if (aptr != nullptr) {
            flag = 1;
            sink->write_value(flag);
            serialize_impl(aptr, sink);
        }
        else {
            auto gptr = ptr.downcast<group>();
            if (gptr != nullptr) {
                flag = 2;
                sink->write_value(flag);
                serialize_impl(gptr, sink);
            }
            else {
                CPPA_LOGF_ERROR("ptr is neither an actor nor a group");
                wr_nullptr();
            }
        }
    }
}

void deserialize_impl(channel_ptr& ptrref, deserializer* source) {
    auto flag = source->read<uint8_t>();
    switch (flag) {
        case 0: {
            ptrref.reset();
            break;
        }
        case 1: {
            actor_ptr tmp;
            deserialize_impl(tmp, source);
            ptrref = tmp;
            break;
        }
        case 2: {
            group_ptr tmp;
            deserialize_impl(tmp, source);
            ptrref = tmp;
            break;
        }
        default: {
            CPPA_LOGF_ERROR("invalid flag while deserializing 'channel_ptr'");
            throw std::runtime_error("invalid flag");
        }
    }
}

void serialize_impl(const any_tuple& tup, serializer* sink) {
    auto tname = tup.tuple_type_names();
    auto uti = get_uniform_type_info_map()->by_uniform_name(tname ? *tname : detail::get_tuple_type_names(*tup.vals()));
    if (uti == nullptr) {
        std::string err = "could not get uniform type info for ";
        err += tname ? *tname : detail::get_tuple_type_names(*tup.vals());
        throw std::runtime_error(err);
    }
    sink->begin_object(uti);
    for (size_t i = 0; i < tup.size(); ++i) {
        tup.type_at(i)->serialize(tup.at(i), sink);
    }
    sink->end_object();
}

void deserialize_impl(any_tuple& atref, deserializer* source) {
    auto uti = source->begin_object();
    auto ptr = uti->new_instance();
    auto ptr_guard = util::make_scope_guard([&] {
        uti->delete_instance(ptr);
    });
    uti->deserialize(ptr, source);
    source->end_object();
    atref = uti->as_any_tuple(ptr);
}

void serialize_impl(const message_header& hdr, serializer* sink) {
    serialize_impl(hdr.sender, sink);
    serialize_impl(hdr.receiver, sink);
    sink->write_value(hdr.id.integer_value());
}

void deserialize_impl(message_header& hdr, deserializer* source) {
    deserialize_impl(hdr.sender, source);
    deserialize_impl(hdr.receiver, source);
    hdr.id = message_id::from_integer_value(source->read<std::uint64_t>());
}

void serialize_impl(const process_information_ptr& ptr, serializer* sink) {
    if (ptr == nullptr) {
        process_information::serialize_invalid(sink);
    }
    else {
        sink->write_value(ptr->process_id());
        sink->write_raw(ptr->node_id().size(), ptr->node_id().data());
    }
}

void deserialize_impl(process_information_ptr& ptr, deserializer* source) {
    process_information::node_id_type nid;
    auto pid = source->read<uint32_t>();
    source->read_raw(process_information::node_id_size, nid.data());
    auto is_zero = [](uint8_t value) { return value == 0; };
    if (pid == 0 && std::all_of(nid.begin(), nid.end(), is_zero)) {
        // invalid process information (nullptr)
        ptr.reset();
    }
    else ptr.reset(new process_information{pid, nid});
}

inline void serialize_impl(const atom_value& val, serializer* sink) {
    sink->write_value(val);
}

inline void deserialize_impl(atom_value& val, deserializer* source) {
    val = source->read<atom_value>();
}

inline void serialize_impl(const util::duration& val, serializer* sink) {
    sink->write_value(static_cast<uint32_t>(val.unit));
    sink->write_value(val.count);
}

inline void deserialize_impl(util::duration& val, deserializer* source) {
    auto unit_val = source->read<uint32_t>();
    auto count_val = source->read<uint32_t>();
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

inline void serialize_impl(bool val, serializer* sink) {
    sink->write_value(static_cast<uint8_t>(val ? 1 : 0));
}

inline void deserialize_impl(bool& val, deserializer* source) {
    val = source->read<uint8_t>() != 0;
}

bool types_equal(const std::type_info* lhs, const std::type_info* rhs) {
    // in some cases (when dealing with dynamic libraries),
    // address can be different although types are equal
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

    any_tuple as_any_tuple(void* instance) const override {
        return make_any_tuple(deref(instance));
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
        sink->write_value(deref(instance));
    }

    void deserialize(void* instance, deserializer* source) const {
        deref(instance) = source->read<T>();
    }

    const char* name() const {
        return static_name();
    }

    any_tuple as_any_tuple(void* instance) const override {
        return make_any_tuple(deref(instance));
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

class buffer_type_info_impl : public uniform_type_info {

 public:

    void serialize(const void* instance, serializer* sink) const {
        auto& val = deref(instance);
        sink->write_value(static_cast<uint32_t>(val.size()));
        sink->write_raw(val.size(), val.data());
    }

    void deserialize(void* instance, deserializer* source) const {
        auto s = source->read<uint32_t>();
        source->read_raw(s, deref(instance));
    }

    const char* name() const {
        return static_name();
    }

    any_tuple as_any_tuple(void* instance) const override {
        return make_any_tuple(deref(instance));
    }

 protected:

    bool equals(const std::type_info& ti) const {
        return ti == typeid(util::buffer);
    }

    bool equals(const void* vlhs, const void* vrhs) const {
        auto& lhs = deref(vlhs);
        auto& rhs = deref(vrhs);
        return    (lhs.empty() && rhs.empty())
               || (   lhs.size() == rhs.size()
                   && memcmp(lhs.data(), rhs.data(), lhs.size()) == 0);
    }

    void* new_instance(const void* ptr) const {
        return (ptr) ? new util::buffer(deref(ptr)) : new util::buffer;
    }

    void delete_instance(void* instance) const {
        delete reinterpret_cast<util::buffer*>(instance);
    }

 private:

    static inline util::buffer& deref(void* ptr) {
        return *reinterpret_cast<util::buffer*>(ptr);
    }

    static inline const util::buffer& deref(const void* ptr) {
        return *reinterpret_cast<const util::buffer*>(ptr);
    }

    static inline const char* static_name() {
        return "@buffer";
    }

};

class default_meta_tuple : public uniform_type_info {

 public:

    default_meta_tuple(const std::string& name) {
        m_name = name;
        auto elements = util::split(name, '+', false);
        auto uti_map = get_uniform_type_info_map();
        CPPA_REQUIRE(elements.size() > 0 && elements.front() == "@<>");
        // ignore first element, because it's always "@<>"
        for (size_t i = 1; i != elements.size(); ++i) {
            try { m_elements.push_back(uti_map->by_uniform_name(elements[i])); }
            catch (std::exception&) {
                CPPA_LOG_ERROR("type name " << elements[i] << " not found");
            }
        }
    }

    void* new_instance(const void* instance = nullptr) const override {
        object_array* result = nullptr;
        if (instance) result = new object_array{*cast(instance)};
        else {
            result = new object_array;
            for (auto uti : m_elements) result->push_back(uti->create());
        }
        result->ref();
        return result;
    }

    void delete_instance(void* ptr) const override {
        cast(ptr)->deref();
    }

    any_tuple as_any_tuple(void* ptr) const override {
        return any_tuple{static_cast<any_tuple::raw_ptr>(cast(ptr))};
    }

    const char* name() const override {
        return m_name.c_str();
    }

    void serialize(const void* ptr, serializer* sink) const override {
        auto& oarr = *cast(ptr);
        for (size_t i = 0; i < m_elements.size(); ++i) {
            m_elements[i]->serialize(oarr.at(i), sink);
        }
    }

    void deserialize(void* ptr, deserializer* source) const override {
        auto& oarr = *cast(ptr);
        for (size_t i = 0; i < m_elements.size(); ++i) {
            m_elements[i]->deserialize(oarr.mutable_at(i), source);
        }
    }

    bool equals(const std::type_info&) const override {
        return false;
    }

    bool equals(const void* instance1, const void* instance2) const override {
        auto& lhs = *cast(instance1);
        auto& rhs = *cast(instance2);
        full_eq_type cmp;
        return std::equal(lhs.begin(), lhs.end(), rhs.begin(), cmp);
    }


 private:

    std::string m_name;
    std::vector<const uniform_type_info*> m_elements;

    inline object_array* cast(void* ptr) const {
        return reinterpret_cast<object_array*>(ptr);
    }

    inline const object_array* cast(const void* ptr) const {
        return reinterpret_cast<const object_array*>(ptr);
    }

};

template<typename T>
void push_native_type(abstract_int_tinfo* m [][2]) {
    m[sizeof(T)][std::is_signed<T>::value ? 1 : 0]->add_native_type(typeid(T));
}

template<typename T0, typename T1, typename... Ts>
void push_native_type(abstract_int_tinfo* m [][2]) {
    push_native_type<T0>(m);
    push_native_type<T1, Ts...>(m);
}

template<typename... Ts>
inline void push_hint(uniform_type_info_map* thisptr) {
    thisptr->insert(create_unique<meta_cow_tuple<Ts...>>());
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
        m_builtin_types[i++] = &m_type_unit;            // @0
        m_builtin_types[i++] = &m_ac_hdl;               // @ac_hdl
        m_builtin_types[i++] = &m_type_actor;           // @actor
        m_builtin_types[i++] = &m_type_atom;            // @atom
        m_builtin_types[i++] = &m_type_buffer;          // @buffer
        m_builtin_types[i++] = &m_type_channel;         // @channel
        m_builtin_types[i++] = &m_cn_hdl;               // @cn_hdl
        m_builtin_types[i++] = &m_type_duration;        // @duration
        m_builtin_types[i++] = &m_type_group;           // @group
        m_builtin_types[i++] = &m_type_header;          // @header
        m_builtin_types[i++] = &m_type_i16;             // @i16
        m_builtin_types[i++] = &m_type_i32;             // @i32
        m_builtin_types[i++] = &m_type_i64;             // @i64
        m_builtin_types[i++] = &m_type_i8;              // @i8
        m_builtin_types[i++] = &m_type_long_double;     // @ldouble
        m_builtin_types[i++] = &m_type_proc;            // @proc
        m_builtin_types[i++] = &m_type_str;             // @str
        m_builtin_types[i++] = &m_type_strmap;          // @strmap
        m_builtin_types[i++] = &m_type_tuple;           // @tuple
        m_builtin_types[i++] = &m_type_u16;             // @u16
        m_builtin_types[i++] = &m_type_u16str;          // @u16str
        m_builtin_types[i++] = &m_type_u32;             // @u32
        m_builtin_types[i++] = &m_type_u32str;          // @u32str
        m_builtin_types[i++] = &m_type_u64;             // @u64
        m_builtin_types[i++] = &m_type_u8;              // @u8
        m_builtin_types[i++] = &m_type_bool;            // bool
        m_builtin_types[i++] = &m_type_double;          // double
        m_builtin_types[i++] = &m_type_float;           // float
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
        // insert default hints
        push_hint<atom_value>(this);
        push_hint<atom_value, std::uint32_t>(this);
        push_hint<atom_value, process_information_ptr>(this);
        push_hint<atom_value, actor_ptr>(this);
        push_hint<atom_value, process_information_ptr, std::uint32_t, std::uint32_t>(this);
        push_hint<atom_value, std::uint32_t, std::string>(this);
    }

    pointer by_rtti(const std::type_info& ti) const {
        util::shared_lock_guard<util::shared_spinlock> guard(m_lock);
        auto res = find_rtti(m_builtin_types, ti);
        return (res) ? res : find_rtti(m_user_types, ti);
    }

    pointer by_uniform_name(const std::string& name) {
        pointer result = nullptr;
        /* lifetime scope of guard */ {
            util::shared_lock_guard<util::shared_spinlock> guard(m_lock);
            result = find_name(m_builtin_types, name);
            result = (result) ? result : find_name(m_user_types, name);
        }
        if (!result && name.compare(0, 4, "@<>+") == 0) {
            // create tuple UTI on-the-fly
            result = insert(create_unique<default_meta_tuple>(name));
        }
        return result; //(res) ? res : find_name(m_user_types, name);
    }

    std::vector<pointer> get_all() const {
        util::shared_lock_guard<util::shared_spinlock> guard(m_lock);
        std::vector<pointer> res;
        res.reserve(m_builtin_types.size() + m_user_types.size());
        res.insert(res.end(), m_builtin_types.begin(), m_builtin_types.end());
        res.insert(res.end(), m_user_types.begin(), m_user_types.end());
        return res;
    }

    pointer insert(std::unique_ptr<uniform_type_info> uti) {
        std::unique_lock<util::shared_spinlock> guard(m_lock);
        auto e = m_user_types.end();
        auto i = std::lower_bound(m_user_types.begin(), e, uti.get(), [](uniform_type_info* lhs, pointer rhs) {
            return strcmp(lhs->name(), rhs->name()) < 0;
        });
        if (i == e) {
            m_user_types.push_back(uti.release());
            return m_user_types.back();
        }
        else {
            if (strcmp(uti->name(), (*i)->name()) == 0) {
                // type already known
                return *i;
            }
            // insert after lower bound (vector is always sorted)
            auto new_pos = std::distance(m_user_types.begin(), i);
            m_user_types.insert(i, uti.release());
            return m_user_types[new_pos];
        }
    }

    ~utim_impl() {
        for (auto ptr : m_user_types) delete ptr;
        m_user_types.clear();
    }

 private:

    typedef std::map<std::string, std::string> strmap;

    uti_impl<process_information_ptr>       m_type_proc;
    uti_impl<io::accept_handle>             m_ac_hdl;
    uti_impl<io::connection_handle>         m_cn_hdl;
    uti_impl<channel_ptr>                   m_type_channel;
    buffer_type_info_impl                   m_type_buffer;
    uti_impl<actor_ptr>                     m_type_actor;
    uti_impl<group_ptr>                     m_type_group;
    uti_impl<any_tuple>                     m_type_tuple;
    uti_impl<util::duration>                m_type_duration;
    uti_impl<message_header>                m_type_header;
    uti_impl<unit_t>                        m_type_unit;
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
    std::array<pointer, 28> m_builtin_types;
    std::vector<uniform_type_info*> m_user_types;
    mutable util::shared_spinlock m_lock;

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
