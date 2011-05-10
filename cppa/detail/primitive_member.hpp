#ifndef PRIMITIVE_MEMBER_HPP
#define PRIMITIVE_MEMBER_HPP

#include "cppa/primitive_type.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/detail/type_to_ptype.hpp"
#include "cppa/util/uniform_type_info_base.hpp"

namespace cppa { namespace detail {

// uniform_type_info implementation for primitive data types.
template<typename T>
class primitive_member : public util::uniform_type_info_base<T>
{

    static constexpr primitive_type ptype = type_to_ptype<T>::ptype;

    static_assert(ptype != pt_null, "T is not a primitive type");

 public:

    void serialize(const void* obj, serializer* s) const
    {
        s->write_value(*reinterpret_cast<const T*>(obj));
    }

    void deserialize(void* obj, deserializer* d) const
    {
        primitive_variant val(d->read_value(ptype));
        *reinterpret_cast<T*>(obj) = std::move(get<T>(val));
    }

};

} } // namespace cppa::detail

#endif // PRIMITIVE_MEMBER_HPP
