#ifndef LIST_MEMBER_HPP
#define LIST_MEMBER_HPP

#include "cppa/util/is_primitive.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

namespace cppa { namespace detail {

template<typename List, bool HasPrimitiveValues>
struct list_member_util
{
    typedef typename List::value_type value_type;
    static constexpr primitive_type vptype = type_to_ptype<value_type>::ptype;
    void operator()(const List& list, serializer* s) const
    {
        s->begin_sequence(list.size());
        for (auto i = list.begin(); i != list.end(); ++i)
        {
            s->write_value(*i);
        }
        s->end_sequence();
    }
    void operator()(List& list, deserializer* d) const
    {
        list.clear();
        auto size = d->begin_sequence();
        for (decltype(size) i = 0; i < size; ++i)
        {
            list.push_back(get<value_type>(d->read_value(vptype)));
        }
        d->end_sequence();
    }
};

template<typename List>
struct list_member_util<List, false>
{
    typedef typename List::value_type value_type;

    const uniform_type_info* m_value_type;

    list_member_util() : m_value_type(uniform_typeid<value_type>())
    {
    }

    void operator()(const List& list, serializer* s) const
    {
        s->begin_sequence(list.size());
        for (auto i = list.begin(); i != list.end(); ++i)
        {
            m_value_type->serialize(&(*i), s);
        }
        s->end_sequence();
    }

    void operator()(List& list, deserializer* d) const
    {
        list.clear();
        value_type tmp;
        auto size = d->begin_sequence();
        for (decltype(size) i = 0; i < size; ++i)
        {
            m_value_type->deserialize(&tmp, d);
            list.push_back(tmp);
        }
        d->end_sequence();
    }
};

template<typename List>
class list_member : public util::abstract_uniform_type_info<List>
{

    typedef typename List::value_type value_type;
    list_member_util<List, util::is_primitive<value_type>::value> m_helper;

 public:

    void serialize(const void* obj, serializer* s) const
    {
        auto& list = *reinterpret_cast<const List*>(obj);
        m_helper(list, s);
    }

    void deserialize(void* obj, deserializer* d) const
    {
        auto& list = *reinterpret_cast<List*>(obj);
        m_helper(list, d);
    }

};

} } // namespace cppa::detail

#endif // LIST_MEMBER_HPP
