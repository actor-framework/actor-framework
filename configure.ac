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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
