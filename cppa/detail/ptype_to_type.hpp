#ifndef PTYPE_TO_TYPE_HPP
#define PTYPE_TO_TYPE_HPP

#include <cstdint>

#include "cppa/primitive_type.hpp"

#include "cppa/util/if_else_type.hpp"
#include "cppa/util/wrapped_type.hpp"

namespace cppa { namespace detail {

// maps the primitive_type PT to the corresponding type
template<primitive_type PT>
struct ptype_to_type :
    // signed integers
    util::if_else_type_c<PT == pt_int8, std::int8_t,
    util::if_else_type_c<PT == pt_int16, std::int16_t,
    util::if_else_type_c<PT == pt_int32, std::int32_t,
    util::if_else_type_c<PT == pt_int64, std::int64_t,
    util::if_else_type_c<PT == pt_uint8, std::uint8_t,
    // unsigned integers
    util::if_else_type_c<PT == pt_uint16, std::uint16_t,
    util::if_else_type_c<PT == pt_uint32, std::uint32_t,
    util::if_else_type_c<PT == pt_uint64, std::uint64_t,
    // floating points
    util::if_else_type_c<PT == pt_float, float,
    util::if_else_type_c<PT == pt_double, double,
    util::if_else_type_c<PT == pt_long_double, long double,
    // strings
    util::if_else_type_c<PT == pt_u8string, std::string,
    util::if_else_type_c<PT == pt_u16string, std::u16string,
    util::if_else_type_c<PT == pt_u32string, std::u32string,
    // default case
    util::wrapped_type<void> > > > > > > > > > > > > > >
{
};

} } // namespace cppa::detail

#endif // PTYPE_TO_TYPE_HPP
