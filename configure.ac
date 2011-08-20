AC_INIT([libcppa], [1.0.0])
AM_INIT_AUTOMAKE

AC_PROG_CXX
AC_PROG_LIBTOOL

AC_LANG([C++])
CPPFLAGS="-Werror --std=c++0x"

AC_CACHE_CHECK(
  [whether C++ compiler supports variadic templates],
  [ac_cv_cpp_variadic_templates],
  AC_COMPILE_IFELSE([[#include "variadic_templates_test.cpp"]],
                    [ac_cv_cpp_variadic_templates=yes],
                    [ac_cv_cpp_variadic_templates=no]))

AC_CACHE_CHECK(
  [whether C++ compiler supports nullptr],
  [ac_cv_cpp_nullptr],
  AC_COMPILE_IFELSE([[static void* myptr = nullptr;]],
                    [ac_cv_cpp_nullptr=yes],
                    [ac_cv_cpp_nullptr=no]))

AS_IF(
  [test "x$ac_cv_cpp_variadic_templates" = "xyes" &&
   test "x$ac_cv_cpp_nullptr" = "xyes"],
  [],
  [AC_MSG_ERROR([at least one required C++ compiler feature is not supported])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
