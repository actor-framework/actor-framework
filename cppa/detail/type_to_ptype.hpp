#ifndef TYPE_TO_PTYPE_HPP
#define TYPE_TO_PTYPE_HPP

#include <cstdint>
#include <type_traits>

#include "cppa/primitive_type.hpp"

#include "cppa/util/apply.hpp"

namespace cppa { namespace detail {

// if (IfStmt == true) ptype = PT; else ptype = Else::ptype;
template<bool IfStmt, primitive_type PT, class Else>
struct if_else_ptype_c
{
    static const primitive_type ptype = PT;
};

template<primitive_type PT, class Else>
struct if_else_ptype_c<false, PT, Else>
{
    static const primitive_type ptype = Else::ptype;
};

// if (Stmt::value == true) ptype = FT; else ptype = Else::ptype;
template<class Stmt, primitive_type PT, class Else>
struct if_else_ptype : if_else_ptype_c<Stmt::value, PT, Else> { };

template<primitive_type PT>
struct wrapped_ptype { static const primitive_type ptype = PT; };

// maps type T the the corresponding fundamental_type
template<typename T>
struct type_to_ptype_impl :
    // signed integers
    if_else_ptype<std::is_same<T, std::int8_t>, pt_int8,
    if_else_ptype<std::is_same<T, std::int16_t>, pt_int16,
    if_else_ptype<std::is_same<T, std::int32_t>, pt_int32,
    if_else_ptype<std::is_same<T, std::int64_t>, pt_int64,
    if_else_ptype<std::is_same<T, std::uint8_t>, pt_uint8,
    // unsigned integers
    if_else_ptype<std::is_same<T, std::uint16_t>, pt_uint16,
    if_else_ptype<std::is_same<T, std::uint32_t>, pt_uint32,
    if_else_ptype<std::is_same<T, std::uint64_t>, pt_uint64,
    // floating points
    if_else_ptype<std::is_same<T, float>, pt_float,
    if_else_ptype<std::is_same<T, double>, pt_double,
    if_else_ptype<std::is_same<T, long double>, pt_long_double,
    // strings
    if_else_ptype<std::is_convertible<T, std::string>, pt_u8string,
    if_else_ptype<std::is_convertible<T, std::u16string>, pt_u16string,
    if_else_ptype<std::is_convertible<T, std::u32string>, pt_u32string,
    // default case
    wrapped_ptype<pt_null> > > > > > > > > > > > > > >
{
};

template<typename T>
struct type_to_ptype
{

    typedef typename util::apply<T,
                                 std::remove_reference,
                                 std::remove_cv        >::type type;

    static const primitive_type ptype = type_to_ptype_impl<type>::ptype;

};

} } // namespace cppa::detail

#endif // TYPE_TO_PTYPE_HPP
