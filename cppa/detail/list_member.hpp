#ifndef LIST_MEMBER_HPP
#define LIST_MEMBER_HPP

#include "cppa/util/uniform_type_info_base.hpp"

namespace cppa { namespace detail {

template<typename List>
class list_member : public util::uniform_type_info_base<List>
{

    typedef typename List::value_type value_type;
    static constexpr primitive_type vptype = type_to_ptype<value_type>::ptype;

    static_assert(vptype != pt_null,
                  "List::value_type is not a primitive data type");

 public:

    void serialize(const void* obj, serializer* s) const
    {
        auto& ls = *reinterpret_cast<const List*>(obj);
        s->begin_sequence(ls.size());
        for (const auto& val : ls)
        {
            s->write_value(val);
        }
        s->end_sequence();
    }

    void deserialize(void* obj, deserializer* d) const
    {
        auto& ls = *reinterpret_cast<List*>(obj);
        size_t ls_size = d->begin_sequence();
        for (size_t i = 0; i < ls_size; ++i)
        {
            primitive_variant val = d->read_value(vptype);
            ls.push_back(std::move(get<value_type>(val)));
        }
        d->end_sequence();
    }

};

} } // namespace cppa::detail

#endif // LIST_MEMBER_HPP
