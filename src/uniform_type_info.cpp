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


#include "cppa/config.hpp"

#include <map>
#include <set>
#include <locale>
#include <string>
#include <atomic>
#include <limits>
#include <cstring>
#include <cstdint>
#include <type_traits>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/object.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/announce.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/void_type.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/addressed_message.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

namespace cppa { namespace detail {

namespace {

inline uniform_type_info_map& uti_map() {
    return *singleton_manager::get_uniform_type_info_map();
}

inline const char* raw_name(const std::type_info& tinfo) {
#ifdef CPPA_WINDOWS
    return tinfo.raw_name();
#else
    return tinfo.name();
#endif
}

template<typename T>
struct is_signed :
       std::integral_constant<bool, std::numeric_limits<T>::is_signed> { };

template<typename T>
inline const char* raw_name() {
    return raw_name(typeid(T));
}

typedef std::set<std::string> string_set;
typedef std::map<int, std::pair<string_set, string_set> > int_name_mapping;

template<typename Int>
inline void push_impl(int_name_mapping& ints, std::true_type) {
    ints[sizeof(Int)].first.insert(raw_name<Int>()); // signed version
}

template<typename Int>
inline void push_impl(int_name_mapping& ints, std::false_type) {
    ints[sizeof(Int)].second.insert(raw_name<Int>()); // unsigned version
}

template<typename Int>
inline void push(int_name_mapping& ints) {
    push_impl<Int>(ints, is_signed<Int>{});
}

template<typename Int0, typename Int1, typename... Ints>
inline void push(int_name_mapping& ints) {
    push_impl<Int0>(ints, is_signed<Int0>{});
    push<Int1, Ints...>(ints);
}

const std::string s_nullptr_type_name{"@0"};

void serialize_nullptr(serializer* sink) {
    sink->begin_object(s_nullptr_type_name);
    sink->end_object();
}

void deserialize_nullptr(deserializer* source) {
    source->begin_object(s_nullptr_type_name);
    source->end_object();
}

} // namespace <anonymous>

class void_type_tinfo : public uniform_type_info {

 public:

    void_type_tinfo() : uniform_type_info(to_uniform_name<util::void_type>()) {}

    bool equals(const std::type_info &tinfo) const {
        return typeid(util::void_type) == tinfo;
    }

 protected:

    void serialize(const void*, serializer* sink) const {
        serialize_nullptr(sink);
    }

    void deserialize(void*, deserializer* source) const {
        std::string cname = source->seek_object();
        if (cname != name()) {
            throw std::logic_error("wrong type name found");
        }
        deserialize_nullptr(source);
    }

    bool equals(const void*, const void*) const {
        return true;
    }

    void* new_instance(const void*) const {
        // const_cast cannot cause any harm, because void_type is immutable
        return const_cast<void*>(static_cast<const void*>(&m_value));
    }

    void delete_instance(void* instance) const {
        CPPA_REQUIRE(instance == &m_value);
        // keep compiler happy (suppress unused argument warning)
        static_cast<void>(instance);
    }

 private:

    util::void_type m_value;

};

class actor_ptr_tinfo : public util::abstract_uniform_type_info<actor_ptr> {

 public:

    static void s_serialize(const actor_ptr& ptr,
                            serializer* sink,
                            const std::string& name) {
        if (ptr == nullptr) {
            serialize_nullptr(sink);
        }
        else {
            if (ptr->is_proxy() == false) {
                singleton_manager::get_actor_registry()->put(ptr->id(), ptr);
            }
            primitive_variant ptup[3];
            ptup[0] = ptr->id();
            ptup[1] = ptr->parent_process().process_id();
            ptup[2] = to_string(ptr->parent_process().node_id());
            sink->begin_object(name);
            sink->write_tuple(3, ptup);
            sink->end_object();
        }
    }

    static void s_deserialize(actor_ptr& ptrref,
                              deserializer* source,
                              const std::string& name) {
        std::string cname = source->seek_object();
        if (cname != name) {
            if (cname == s_nullptr_type_name) {
                deserialize_nullptr(source);
                ptrref.reset();
            }
            else {
                throw std::logic_error("wrong type name found");
            }
        }
        else {
            primitive_variant ptup[3];
            primitive_type ptypes[] = { pt_uint32, pt_uint32, pt_u8string };
            source->begin_object(cname);
            source->read_tuple(3, ptypes, ptup);
            source->end_object();
            const std::string& nstr = get<std::string>(ptup[2]);
            // local actor?
            auto pinf = process_information::get();
            if (   pinf->process_id() == get<std::uint32_t>(ptup[1])
                && cppa::equal(nstr, pinf->node_id())) {
                auto id = get<std::uint32_t>(ptup[0]);
                ptrref = singleton_manager::get_actor_registry()->get(id);
                //ptrref = actor::by_id(get<std::uint32_t>(ptup[0]));
            }
            else {
                /*
                actor_proxy_cache::key_tuple key;
                std::get<0>(key) = get<std::uint32_t>(ptup[0]);
                std::get<1>(key) = get<std::uint32_t>(ptup[1]);
                node_id_from_string(nstr, std::get<2>(key));
                ptrref = detail::get_actor_proxy_cache().get(key);
                */
                process_information::node_id_type nid;
                node_id_from_string(nstr, nid);
                ptrref = detail::get_actor_proxy_cache().get(get<std::uint32_t>(ptup[0]),
                                                             get<std::uint32_t>(ptup[1]),
                                                             nid);
            }
        }
    }

 protected:

    void serialize(const void* ptr, serializer* sink) const {
        s_serialize(*reinterpret_cast<const actor_ptr*>(ptr), sink, name());
    }

    void deserialize(void* ptr, deserializer* source) const {
        s_deserialize(*reinterpret_cast<actor_ptr*>(ptr), source, name());
    }

};

class group_ptr_tinfo : public util::abstract_uniform_type_info<group_ptr> {

 public:

    static void s_serialize(const group_ptr& ptr,
                            serializer* sink,
                            const std::string& name) {
        if (ptr == nullptr) {
            serialize_nullptr(sink);
        }
        else {
            sink->begin_object(name);
            sink->write_value(ptr->module_name());
            sink->write_value(ptr->identifier());
            sink->end_object();
        }
    }

    static void s_deserialize(group_ptr& ptrref,
                              deserializer* source,
                              const std::string& name) {
        std::string cname = source->seek_object();
        if (cname != name) {
            if (cname == s_nullptr_type_name) {
                deserialize_nullptr(source);
                ptrref.reset();
            }
            else {
                throw std::logic_error("wrong type name found");
            }
        }
        else {
            source->begin_object(name);
            auto modname = source->read_value(pt_u8string);
            auto groupid = source->read_value(pt_u8string);
            ptrref = group::get(get<std::string>(modname),
                                get<std::string>(groupid));
            source->end_object();
        }
    }

 protected:

    void serialize(const void* ptr, serializer* sink) const {
        s_serialize(*reinterpret_cast<const group_ptr*>(ptr),
                    sink,
                    name());
    }

    void deserialize(void* ptr, deserializer* source) const {
        s_deserialize(*reinterpret_cast<group_ptr*>(ptr), source, name());
    }

};

class channel_ptr_tinfo : public util::abstract_uniform_type_info<channel_ptr> {

    std::string group_ptr_name;
    std::string actor_ptr_name;

 public:

    static void s_serialize(const channel_ptr& ptr,
                            serializer* sink,
                            const std::string& channel_type_name,
                            const std::string& actor_ptr_type_name,
                            const std::string& group_ptr_type_name) {
        sink->begin_object(channel_type_name);
        if (ptr == nullptr) {
            serialize_nullptr(sink);
        }
        else {
            group_ptr gptr;
            auto aptr = ptr.downcast<actor>();
            if (aptr) {
                actor_ptr_tinfo::s_serialize(aptr, sink, actor_ptr_type_name);
            }
            else if ((gptr = ptr.downcast<group>())) {
                group_ptr_tinfo::s_serialize(gptr, sink, group_ptr_type_name);
            }
            else {
                throw std::logic_error("channel is neither "
                                       "an actor nor a group");
            }
        }
        sink->end_object();
    }

    static void s_deserialize(channel_ptr& ptrref,
                              deserializer* source,
                              const std::string& name,
                              const std::string& actor_ptr_type_name,
                              const std::string& group_ptr_type_name) {
        std::string cname = source->seek_object();
        if (cname != name) {
            throw std::logic_error("wrong type name found");
        }
        source->begin_object(cname);
        std::string subobj = source->peek_object();
        if (subobj == actor_ptr_type_name) {
            actor_ptr tmp;
            actor_ptr_tinfo::s_deserialize(tmp, source, actor_ptr_type_name);
            ptrref = tmp;
        }
        else if (subobj == group_ptr_type_name) {
            group_ptr tmp;
            group_ptr_tinfo::s_deserialize(tmp, source, group_ptr_type_name);
            ptrref = tmp;
        }
        else if (subobj == s_nullptr_type_name) { (void) source->seek_object();
            deserialize_nullptr(source);
            ptrref.reset();
        }
        else {
            throw std::logic_error("unexpected type name: " + subobj);
        }
        source->end_object();
    }

 protected:

    void serialize(const void* instance, serializer* sink) const {
        s_serialize(*reinterpret_cast<const channel_ptr*>(instance),
                    sink,
                    name(),
                    actor_ptr_name,
                    group_ptr_name);
    }

    void deserialize(void* instance, deserializer* source) const {
        s_deserialize(*reinterpret_cast<channel_ptr*>(instance),
                      source,
                      name(),
                      actor_ptr_name,
                      group_ptr_name);
    }

 public:

    channel_ptr_tinfo() : group_ptr_name(to_uniform_name(typeid(group_ptr)))
                        , actor_ptr_name(to_uniform_name(typeid(actor_ptr))) { }

};

class any_tuple_tinfo : public util::abstract_uniform_type_info<any_tuple> {

 public:

    static void s_serialize(const any_tuple& atup,
                            serializer* sink,
                            const std::string& name) {
        sink->begin_object(name);
        sink->begin_sequence(atup.size());
        for (size_t i = 0; i < atup.size(); ++i) {
            atup.type_at(i)->serialize(atup.at(i), sink);
        }
        sink->end_sequence();
        sink->end_object();
    }

    static void s_deserialize(any_tuple& atref,
                              deserializer* source,
                              const std::string& name) {
        auto result = new detail::object_array;
        auto str = source->seek_object();
        if (str != name) throw std::logic_error("invalid type found: " + str);
        source->begin_object(str);
        size_t tuple_size = source->begin_sequence();
        for (size_t i = 0; i < tuple_size; ++i) {
            auto tname = source->peek_object();
            auto utype = uniform_type_info::from(tname);
            result->push_back(utype->deserialize(source));
        }
        source->end_sequence();
        source->end_object();
        atref = any_tuple{result};
    }

 protected:

    void serialize(const void* instance, serializer* sink) const {
        s_serialize(*reinterpret_cast<const any_tuple*>(instance),sink,name());
    }

    void deserialize(void* instance, deserializer* source) const {
        s_deserialize(*reinterpret_cast<any_tuple*>(instance), source, name());
    }

};

class addr_msg_tinfo : public util::abstract_uniform_type_info<addressed_message> {

    std::string any_tuple_name;
    std::string actor_ptr_name;
    std::string group_ptr_name;
    std::string channel_ptr_name;

 public:

    virtual void serialize(const void* instance, serializer* sink) const {
        auto& msg = *reinterpret_cast<const addressed_message*>(instance);
        auto& data = msg.content();
        sink->begin_object(name());
        actor_ptr_tinfo::s_serialize(msg.sender(), sink, actor_ptr_name);
        channel_ptr_tinfo::s_serialize(msg.receiver(),
                                       sink,
                                       channel_ptr_name,
                                       actor_ptr_name,
                                       group_ptr_name);
        any_tuple_tinfo::s_serialize(data, sink, any_tuple_name);
        sink->end_object();
    }

    virtual void deserialize(void* instance, deserializer* source) const {
        auto tname = source->seek_object();
        if (tname != name()) throw 42;
        source->begin_object(tname);
        auto& msg = *reinterpret_cast<addressed_message*>(instance);
        //actor_ptr sender;
        //channel_ptr receiver;
        //any_tuple content;
        actor_ptr_tinfo::s_deserialize(msg.sender(), source, actor_ptr_name);
        channel_ptr_tinfo::s_deserialize(msg.receiver(),
                                         source,
                                         channel_ptr_name,
                                         actor_ptr_name,
                                         group_ptr_name);
        any_tuple_tinfo::s_deserialize(msg.content(), source, any_tuple_name);
        source->end_object();
        //*reinterpret_cast<any_tuple*>(instance) = any_tuple(sender,
        //                                                    receiver,
        //                                                    content);
    }

    addr_msg_tinfo() : any_tuple_name(to_uniform_name<any_tuple>())
                     , actor_ptr_name(to_uniform_name<actor_ptr>())
                     , group_ptr_name(to_uniform_name<group_ptr>())
                     , channel_ptr_name(to_uniform_name<channel_ptr>()) { }

};

class process_info_ptr_tinfo : public util::abstract_uniform_type_info<process_information_ptr> {

    typedef process_information_ptr ptr_type;

 public:

    virtual void serialize(const void* instance, serializer* sink) const {
        auto& ptr = *reinterpret_cast<const ptr_type*>(instance);
        if (ptr == nullptr) {
            serialize_nullptr(sink);
        }
        else {
            primitive_variant ptup[2];
            ptup[0] = ptr->process_id();
            ptup[1] = to_string(ptr->node_id());
            sink->begin_object(name());
            sink->write_tuple(2, ptup);
            sink->end_object();
        }
    }

    virtual void deserialize(void* instance, deserializer* source) const {
        auto& ptrref = *reinterpret_cast<ptr_type*>(instance);
        std::string cname = source->seek_object();
        if (cname != name()) {
            if (cname == s_nullptr_type_name) {
                deserialize_nullptr(source);
                ptrref.reset();
            }
            else {
                throw std::logic_error("wrong type name found");
            }
        }
        else {
            primitive_variant ptup[2];
            primitive_type ptypes[] = { pt_uint32, pt_u8string };
            source->begin_object(cname);
            source->read_tuple(2, ptypes, ptup);
            source->end_object();
            process_information::node_id_type nid;
            node_id_from_string(get<std::string>(ptup[1]), nid);
            ptrref.reset(new process_information{get<std::uint32_t>(ptup[0]), nid});
        }
    }

};

class atom_value_tinfo : public util::abstract_uniform_type_info<atom_value> {

 public:

    virtual void serialize(const void* instance, serializer* sink) const {
        auto val = reinterpret_cast<const atom_value*>(instance);
        sink->begin_object(name());
        sink->write_value(static_cast<std::uint64_t>(*val));
        sink->end_object();
    }

    virtual void deserialize(void* instance, deserializer* source) const {
        auto val = reinterpret_cast<atom_value*>(instance);
        auto tname = source->seek_object();
        if (tname != name()) throw 42;
        source->begin_object(tname);
        auto ptval = source->read_value(pt_uint64);
        source->end_object();
        *val = static_cast<atom_value>(get<std::uint64_t>(ptval));
    }

};

class duration_tinfo : public util::abstract_uniform_type_info<util::duration> {

    virtual void serialize(const void* instance, serializer* sink) const {
        auto val = reinterpret_cast<const util::duration*>(instance);
        sink->begin_object(name());
        sink->write_value(static_cast<std::uint32_t>(val->unit));
        sink->write_value(val->count);
        sink->end_object();
    }

    virtual void deserialize(void* instance, deserializer* source) const {
        auto val = reinterpret_cast<util::duration*>(instance);
        auto tname = source->seek_object();
        if (tname != name()) throw 42;
        source->begin_object(tname);
        auto unit_val = source->read_value(pt_uint32);
        auto count_val = source->read_value(pt_uint32);
        source->end_object();
        switch (get<std::uint32_t>(unit_val)) {
            case 1:
                val->unit = util::time_unit::seconds;
                break;

            case 1000:
                val->unit = util::time_unit::milliseconds;
                break;

            case 1000000:
                val->unit = util::time_unit::microseconds;
                break;

            default:
                val->unit = util::time_unit::none;
                break;
        }
        val->count = get<std::uint32_t>(count_val);
    }

};

template<typename T>
class int_tinfo : public detail::default_uniform_type_info_impl<T> {

 public:

    bool equals(const std::type_info& tinfo) const {
        // TODO: string comparsion sucks & is slow; find a nicer solution
        auto map_iter = uti_map().int_names().find(sizeof(T));
        const string_set& st = is_signed<T>::value ? map_iter->second.first
                                                   : map_iter->second.second;
        auto x = raw_name(tinfo);
        return std::any_of(st.begin(), st.end(),
                           [&x](const std::string& y) { return x == y; });
    }

};

class uniform_type_info_map_helper {

 public:

    uniform_type_info_map_helper(uniform_type_info_map* umap) : d(umap) { }

    template<typename T>
    inline void insert_int() {
        d->insert(d->m_ints[sizeof(T)].first, new int_tinfo<T>);
    }

    template<typename T>
    inline void insert_uint() {
        d->insert(d->m_ints[sizeof(T)].second, new int_tinfo<T>);
    }

    template<typename T>
    inline void insert_builtin() {
        d->insert({raw_name<T>()}, new default_uniform_type_info_impl<T>);
    }

 private:

    uniform_type_info_map* d;

};

uniform_type_info_map::uniform_type_info_map() {
    // inserts all compiler generated raw-names to m_ings
    push<char,                  signed char,
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
         intptr_t                               >(m_ints);
    uniform_type_info_map_helper helper{this};
    // inserts integer type infos
    helper.insert_int<std::int8_t>();
    helper.insert_int<std::int16_t>();
    helper.insert_int<std::int32_t>();
    helper.insert_int<std::int64_t>();
    helper.insert_uint<std::uint8_t>();
    helper.insert_uint<std::uint16_t>();
    helper.insert_uint<std::uint32_t>();
    helper.insert_uint<std::uint64_t>();
    // insert type infos for "built-in" types
    helper.insert_builtin<float>();
    helper.insert_builtin<double>();
    helper.insert_builtin<std::string>();
    helper.insert_builtin<std::u16string>();
    helper.insert_builtin<std::u32string>();
    // insert cppa types
    insert({raw_name<util::duration>()}, new duration_tinfo);
    insert({raw_name<any_tuple>()}, new any_tuple_tinfo);
    insert({raw_name<actor_ptr>()}, new actor_ptr_tinfo);
    insert({raw_name<group_ptr>()}, new group_ptr_tinfo);
    insert({raw_name<channel_ptr>()}, new channel_ptr_tinfo);
    insert({raw_name<atom_value>()}, new atom_value_tinfo);
    insert({raw_name<detail::addressed_message>()}, new addr_msg_tinfo);
    insert({raw_name<util::void_type>()}, new void_type_tinfo);
    insert({raw_name<process_information_ptr>()}, new process_info_ptr_tinfo);
}

uniform_type_info_map::~uniform_type_info_map() {
    m_by_rname.clear();
    for (auto& kvp : m_by_uname) {
        delete kvp.second;
        kvp.second = nullptr;
    }
    m_by_uname.clear();
}

const uniform_type_info* uniform_type_info_map::by_raw_name(const std::string& name) const {
    auto i = m_by_rname.find(name);
    if (i != m_by_rname.end()) {
        return i->second;
    }
    return nullptr;
}

const uniform_type_info* uniform_type_info_map::by_uniform_name(const std::string& name) const {
    auto i = m_by_uname.find(name);
    if (i != m_by_uname.end()) {
        return i->second;
    }
    return nullptr;
}

bool uniform_type_info_map::insert(const std::set<std::string>& raw_names,
                                   uniform_type_info* what) {
    if (m_by_uname.count(what->name()) > 0) {
        delete what;
        return false;
    }
    m_by_uname.insert(std::make_pair(what->name(), what));
    for (auto& plain_name : raw_names) {
        if (!m_by_rname.insert(std::make_pair(plain_name, what)).second) {
            std::string error_str = plain_name;
            error_str += " already mapped to an uniform_type_info";
            throw std::runtime_error(error_str);
        }
    }
    return true;
}

std::vector<const uniform_type_info*> uniform_type_info_map::get_all() const {
    std::vector<const uniform_type_info*> result;
    result.reserve(m_by_uname.size());
    for (const uti_map_type::value_type& i : m_by_uname) {
        result.push_back(i.second);
    }
    return std::move(result);
}

} } // namespace cppa::detail

namespace cppa {

bool announce(const std::type_info& tinfo, uniform_type_info* utype) {
    return detail::uti_map().insert({detail::raw_name(tinfo)}, utype);
}

uniform_type_info::uniform_type_info(const std::string& str) : m_name(str) { }

uniform_type_info::~uniform_type_info() {
}

object uniform_type_info::create() const {
    return {new_instance(), this};
}

const uniform_type_info* uniform_type_info::from(const std::type_info& tinf) {
    auto result = detail::uti_map().by_raw_name(detail::raw_name(tinf));
    if (result == nullptr) {
        std::string error = "uniform_type_info::by_type_info(): ";
        error += detail::to_uniform_name(tinf);
        error += " is an unknown typeid name";
        throw std::runtime_error(error);
    }
    return result;
}

const uniform_type_info* uniform_type_info::from(const std::string& name) {
    auto result = detail::uti_map().by_uniform_name(name);
    if (result == nullptr) {
        throw std::runtime_error(name + " is an unknown typeid name");
    }
    return result;
}

object uniform_type_info::deserialize(deserializer* from) const {
    auto ptr = new_instance();
    deserialize(ptr, from);
    return {ptr, this};
}

std::vector<const uniform_type_info*> uniform_type_info::instances() {
    return detail::uti_map().get_all();
}

const uniform_type_info* uniform_typeid(const std::type_info& tinfo) {
    return uniform_type_info::from(tinfo);
}

} // namespace cppa
