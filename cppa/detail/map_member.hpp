#ifndef MAP_MEMBER_HPP
#define MAP_MEMBER_HPP

#include "cppa/util/uniform_type_info_base.hpp"
#include "cppa/detail/primitive_member.hpp"
#include "cppa/detail/pair_member.hpp"

namespace cppa { namespace detail {

// matches value_type of std::set
template<typename T>
struct meta_value_type
{
    primitive_member<T> impl;
    void serialize_value(const T& what, serializer* s) const
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

// matches value_type of std::map
template<typename T1, typename T2>
struct meta_value_type<std::pair<const T1, T2>>
{
    pair_member<T1, T2> impl;
    void serialize_value(const std::pair<const T1, T2>& what, serializer* s) const
    {
        std::pair<T1, T2> p(what.first, what.second);
        impl.serialize(&p, s);
    }
    template<typename M>
    void deserialize_and_insert(M& map, deserializer* d) const
    {
        std::pair<T1, T2> p;
        impl.deserialize(&p, d);
        std::pair<const T1, T2> v(std::move(p.first), std::move(p.second));
        map.insert(std::move(v));
    }
};

template<typename Map>
class map_member : public util::uniform_type_info_base<Map>
{

    typedef typename Map::key_type key_type;
    typedef typename Map::value_type value_type;

    meta_value_type<value_type> m_value_type_meta;

 public:

    void serialize(const void* obj, serializer* s) const
    {
        auto& mp = *reinterpret_cast<const Map*>(obj);
        s->begin_sequence(mp.size());
        for (const auto& val : mp)
        {
            m_value_type_meta.serialize_value(val, s);
        }
        s->end_sequence();
    }

    void deserialize(void* obj, deserializer* d) const
    {
        auto& mp = *reinterpret_cast<Map*>(obj);
        size_t mp_size = d->begin_sequence();
        for (size_t i = 0; i < mp_size; ++i)
        {
            m_value_type_meta.deserialize_and_insert(mp, d);
        }
        d->end_sequence();
    }

};

} } // namespace cppa::detail

#endif // MAP_MEMBER_HPP
