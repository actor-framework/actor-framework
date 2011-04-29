#ifndef IF_ELSE_TYPE_HPP
#define IF_ELSE_TYPE_HPP

#include "cppa/util/wrapped_type.hpp"

namespace cppa { namespace util {

// if (IfStmt == true) type = T; else type = Else::type;
template<bool IfStmt, typename T, class Else>
struct if_else_type_c
{
    typedef T type;
};

template<typename T, class Else>
struct if_else_type_c<false, T, Else>
{
    typedef typename Else::type type;
};

// if (Stmt::value == true) type = T; else type = Else::type;
template<class Stmt, typename T, class Else>
struct if_else_type : if_else_type_c<Stmt::value, T, Else> { };

} } // namespace cppa::util

#endif // IF_ELSE_TYPE_HPP
