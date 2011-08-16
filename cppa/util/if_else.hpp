#ifndef IF_ELSE_HPP
#define IF_ELSE_HPP

#include "cppa/util/wrapped.hpp"

namespace cppa { namespace util {

// if (IfStmt == true) type = T; else type = Else::type;
/**
 * @brief A conditinal expression for types that allows
 *        nested statements (unlike std::conditional).
 *
 * @c type is defined as @p T if <tt>IfStmt == true</tt>;
 * otherwise @c type is defined as @p Else::type.
 */
template<bool IfStmt, typename T, class Else>
struct if_else_c
{
    typedef T type;
};

template<typename T, class Else>
struct if_else_c<false, T, Else>
{
    typedef typename Else::type type;
};

// if (Stmt::value == true) type = T; else type = Else::type;
template<class Stmt, typename T, class Else>
struct if_else : if_else_c<Stmt::value, T, Else> { };

} } // namespace cppa::util

#endif // IF_ELSE_HPP
