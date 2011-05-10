#ifndef PAIR_MEMBER_HPP
#define PAIR_MEMBER_HPP

#include <utility>
#include "cppa/util/uniform_type_info_base.hpp"

namespace cppa { namespace detail {

template<typename T1, typename T2>
class pair_member : public util::uniform_type_info_base<std::pair<T1,T2>>
{

    static constexpr primitive_type ptype1 = type_to_ptype<T1>::ptype;
    static constexpr primitive_type ptype2 = type_to_ptype<T2>::ptype;

    static_assert(ptype1 != pt_null, "T1 is not a primitive type");
    static_assert(ptype2 != pt_null, "T2 is not a primitive type");

    typedef std::pair<T1, T2> pair_type;

 public:

    void serialize(const void* obj, serializer* s) const
    {
        auto& p = *reinterpret_cast<const pair_type*>(obj);
        primitive_variant values[2] = { p.first, p.second };
        s->write_tuple(2, values);
    }

    void deserialize(void* obj, deserializer* d) const
    {
        primitive_type ptypes[2] = { ptype1, ptype2 };
        primitive_variant values[2];
        d->read_tuple(2, ptypes, values);
        auto& p = *reinterpret_cast<pair_type*>(obj);
        p.first = std::move(get<T1>(values[0]));
        p.second = std::move(get<T2>(values[1]));
    }

};

} } // namespace cppa::detail

#endif // PAIR_MEMBER_HPP
