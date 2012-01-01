#ifndef MAP_MEMBER_HPP
#define MAP_MEMBER_HPP

#include <type_traits>
#include "cppa/detail/pair_member.hpp"
#include "cppa/detail/primitive_member.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

namespace cppa { namespace detail {

template<typename T>
struct is_pair { static const bool value = false; };

template<typename First, typename Second>
struct is_pair< std::pair<First, Second> > { static const bool value = true; };

template<typename T, bool IsPair, bool IsPrimitive>
struct map_member_util;

// std::set with primitive value
template<typename T>
struct map_member_util<T, false, true>
{
    primitive_member<T> impl;

    void serialize_value(T const& what, serializer* s) const
    {
        impl.serialize(&what, s);
    }

    template<typename M>
    void deserialize_and_insert(M& map, deserializer* d) const
    {
        T value;
        impl.deserialize(&value, d);
        map.insert(std::move(value));
    }
};

// std::set with non-primitive value
template<typename T>
struct map_member_util<T, false, false>
{
    uniform_type_info const* m_type;

    map_member_util() : m_type(uniform_typeid<T>())
    {
    }

    void serialize_value(T const& what, serializer* s) const
    {
        m_type->serialize(&what, s);
    }

    template<typename M>
    void deserialize_and_insert(M& map, deserializer* d) const
    {
        T value;
        m_type->deserialize(&value, d);
        map.insert(std::move(value));
    }
};

// std::map
template<typename T>
struct map_member_util<T, true, false>
{

    // std::map defines its first half of value_type as const
    typedef typename std::remove_const<typename T::first_type>::type first_type;
    typedef typename T::second_type second_type;

    pair_member<first_type, second_type> impl;


    void serialize_value(T const& what, serializer* s) const
    {
        // impl needs a pair without const modifier
        std::pair<first_type, second_type> p(what.first, what.second);
        impl.serialize(&p, s);
    }

    template<typename M>
    void deserialize_and_insert(M& map, deserializer* d) const
    {
        std::pair<first_type, second_type> p;
        impl.deserialize(&p, d);
        std::pair<const first_type, second_type> v(std::move(p.first),
                                                   std::move(p.second));
        map.insert(std::move(v));
    }

};

template<typename Map>
class map_member : public util::abstract_uniform_type_info<Map>
{

    typedef typename Map::key_type key_type;
    typedef typename Map::value_type value_type;

    map_member_util<value_type,
                    is_pair<value_type>::value,
                    util::is_primitive<value_type>::value> m_helper;

 public:

    void serialize(void const* obj, serializer* s) const
    {
        auto& mp = *reinterpret_cast<Map const*>(obj);
        s->begin_sequence(mp.size());
        for (auto const& val : mp)
        {
            m_helper.serialize_value(val, s);
        }
        s->end_sequence();
    }

    void deserialize(void* obj, deserializer* d) const
    {
        auto& mp = *reinterpret_cast<Map*>(obj);
        mp.clear();
        size_t mp_size = d->begin_sequence();
        for (size_t i = 0; i < mp_size; ++i)
        {
            m_helper.deserialize_and_insert(mp, d);
        }
        d->end_sequence();
    }

};

} } // namespace cppa::detail

#endif // MAP_MEMBER_HPP
