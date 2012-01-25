AC_PREREQ([2.6])
AC_INIT([libcppa], [0.1])
AM_INIT_AUTOMAKE(@PACKAGE_NAME@, @PACKAGE_VERSION@)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src])

AC_PROG_CXX

# check for boost and boost_thread
AX_BOOST_BASE([1.42.0])
AX_BOOST_THREAD

AC_PROG_LIBTOOL

# check for C++0x compatibility
AC_LANG([C++])
ORIGINAL_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -Werror -std=c++0x"

AC_CACHE_CHECK(
  [whether C++ compiler supports variadic templates],
  [ac_cv_cpp_variadic_templates],
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#include "variadic_templates_test.cpp"]])],
                    [ac_cv_cpp_variadic_templates=yes],
                    [ac_cv_cpp_variadic_templates=no]))

#AC_CACHE_CHECK(
#  [whether C++ compiler supports nullptr],
#  [ac_cv_cpp_nullptr],
#  AC_COMPILE_IFELSE([AC_LANG_SOURCE([[static void* myptr = nullptr;]])],
#                    [ac_cv_cpp_nullptr=yes],
#                    [ac_cv_cpp_nullptr=no]))

AS_IF(
  [test "x$ac_cv_cpp_variadic_templates" = "xyes" ],
  [],
  [AC_MSG_ERROR([at least one required C++ compiler feature is not supported])])

CPPFLAGS="$ORIGINAL_CPPFLAGS"

# environment
AC_ARG_VAR([NO_VERSIONED_INCLUDE_DIR], [set this to 1 in order to install headers into <prefix>/cppa/ rather than into <prefix>/cppa/<version>/cppa/])
AM_CONDITIONAL([VERSIONED_INCLUDE_DIR], [test "x$NO_VERSIONED_INCLUDE_DIR" != "x1"])

AC_CONFIG_FILES([Makefile unit_testing/Makefile examples/Makefile benchmarks/Makefile libcppa.pc])
AC_OUTPUT
