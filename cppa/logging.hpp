/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_LOGGING_HPP
#define CPPA_LOGGING_HPP

#include <sstream>

#include "cppa/detail/demangle.hpp"

/*
 * To enable logging, you have to define CPPA_DEBUG. This enables
 * CPPA_LOG_ERROR messages. To enable more debugging output, you can define
 * CPPA_LOG_LEVEL to:
 * 1: + warning
 * 2: + info
 * 3: + debug
 * 4: + trace (prints for each logged method entry and exit message)
 *
 * Note: this logger emits log4j style XML output; logs are best viewed
 *       using a log4j viewer, e.g., http://code.google.com/p/otroslogviewer/
 *
 */
namespace cppa {

namespace detail { class singleton_manager; }

class logging {

    friend class detail::singleton_manager;

 public:

    virtual void log(const char* level,
                     const char* class_name,
                     const char* function_name,
                     const char* file_name,
                     int line_num,
                     const std::string& msg    ) = 0;

    static logging* instance();

    class trace_helper {

     public:

        inline trace_helper(std::string class_name,
                            const char* fun_name,
                            const char* file_name,
                            int line_num,
                            const std::string& msg)
        : m_class(std::move(class_name)), m_fun_name(fun_name)
        , m_file_name(file_name), m_line_num(line_num) {
            logging::instance()->log("TRACE  ", m_class.c_str(), fun_name,
                                     file_name, line_num, "ENTRY " + msg);
        }

        inline ~trace_helper() {
            logging::instance()->log("TRACE  ", m_class.c_str(), m_fun_name,
                                     m_file_name, m_line_num, "EXIT");
        }

     private:

        std::string m_class;
        const char* m_fun_name;
        const char* m_file_name;
        int m_line_num;

    };

 protected:

    virtual ~logging();

    static logging* create_singleton();

    virtual void initialize() = 0;

    virtual void destroy() = 0;

    inline void dispose() { delete this; }

};

} // namespace cppa

#define CPPA_VOID_STMT static_cast<void>(0)

#ifndef CPPA_LOG_LEVEL

#define CPPA_LOG_ERROR(unused) CPPA_VOID_STMT
#define CPPA_LOG_ERROR_IF(unused1,unused2) CPPA_VOID_STMT
#define CPPA_LOGF_ERROR(unused) CPPA_VOID_STMT
#define CPPA_LOGF_ERROR_IF(unused1,unused2) CPPA_VOID_STMT

#define CPPA_LOG_WARNING(unused) CPPA_VOID_STMT
#define CPPA_LOG_WARNING_IF(unused1,unused2) CPPA_VOID_STMT
#define CPPA_LOGF_WARNING(unused) CPPA_VOID_STMT
#define CPPA_LOGF_WARNING_IF(unused1,unused2) CPPA_VOID_STMT

#define CPPA_LOG_INFO(unused) CPPA_VOID_STMT
#define CPPA_LOG_INFO_IF(unused1,unused2) CPPA_VOID_STMT
#define CPPA_LOGF_INFO(unused) CPPA_VOID_STMT
#define CPPA_LOGF_INFO_IF(unused1,unused2) CPPA_VOID_STMT

#define CPPA_LOG_DEBUG(unused) CPPA_VOID_STMT
#define CPPA_LOG_DEBUG_IF(unused1,unused2) CPPA_VOID_STMT
#define CPPA_LOGF_DEBUG(unused) CPPA_VOID_STMT
#define CPPA_LOGF_DEBUG_IF(unused1,unused2) CPPA_VOID_STMT

#define CPPA_LOG_TRACE(unused) CPPA_VOID_STMT
#define CPPA_LOGF_TRACE(unused) CPPA_VOID_STMT

#else

#define CPPA_DO_LOG_FUN(level, message) {                                      \
    std::ostringstream scoped_oss; scoped_oss << message;                      \
    ::cppa::detail::logging::instance()->log(                                  \
        level, "NONE",                                                         \
        __FUNCTION__, __FILE__, __LINE__, scoped_oss.str());                   \
} CPPA_VOID_STMT

#define CPPA_DO_LOG_MEMBER_FUN(level, message) {                               \
    std::ostringstream scoped_oss; scoped_oss << message;                      \
    ::cppa::detail::logging::instance()->log(                                  \
        level, ::cppa::detail::demangle(typeid(*this)).c_str(),                \
        __FUNCTION__, __FILE__, __LINE__, scoped_oss.str());                   \
} CPPA_VOID_STMT

#define CPPA_LOG_ERROR(message) CPPA_DO_LOG_MEMBER_FUN("ERROR  ", message)
#define CPPA_LOGF_ERROR(message) CPPA_DO_LOG_FUN("ERROR  ", message)

#if CPPA_LOG_LEVEL > 0
#  define CPPA_LOG_WARNING(message) CPPA_DO_LOG_MEMBER_FUN("WARNING", message)
#  define CPPA_LOGF_WARNING(message) CPPA_DO_LOG_FUN("WARNING", message)
#else
#  define CPPA_LOG_WARNING(unused) CPPA_VOID_STMT
#  define CPPA_LOGF_WARNING(unused) CPPA_VOID_STMT
#endif

#if CPPA_LOG_LEVEL > 1
#  define CPPA_LOG_INFO(message) CPPA_DO_LOG_MEMBER_FUN("INFO   ", message)
#  define CPPA_LOGF_INFO(message) CPPA_DO_LOG_FUN("INFO   ", message)
#else
#  define CPPA_LOG_INFO(unused) CPPA_VOID_STMT
#  define CPPA_LOGF_INFO(unused) CPPA_VOID_STMT
#endif

#if CPPA_LOG_LEVEL > 2
#  define CPPA_LOG_DEBUG(message) CPPA_DO_LOG_MEMBER_FUN("DEBUG  ", message)
#  define CPPA_LOGF_DEBUG(message) CPPA_DO_LOG_FUN("DEBUG  ", message)
#else
#  define CPPA_LOG_DEBUG(unused) CPPA_VOID_STMT
#  define CPPA_LOGF_DEBUG(unused) CPPA_VOID_STMT
#endif

#if CPPA_LOG_LEVEL > 3
#  define CPPA_RPAREN )
#  define CPPA_LPAREN (
#  define CPPA_GET(what) what
#  define CPPA_CONCAT_I(lhs,rhs) lhs ## rhs
#  define CPPA_CONCAT(lhs,rhs) CPPA_CONCAT_I(lhs,rhs)
#  define CPPA_CONCATL(lhs) CPPA_CONCAT(lhs, __LINE__)
#  define CPPA_LOG_TRACE(message)                                              \
    ::std::ostringstream CPPA_CONCATL(cppa_trace_helper_) ;                    \
    CPPA_CONCATL(cppa_trace_helper_) << message ;                              \
    ::cppa::detail::logging::trace_helper CPPA_CONCATL(cppa_fun_trace_helper_) \
        CPPA_LPAREN ::cppa::detail::demangle                                   \
        CPPA_LPAREN typeid CPPA_LPAREN decltype CPPA_LPAREN *this              \
        CPPA_RPAREN        CPPA_RPAREN          CPPA_RPAREN ,                  \
          __func__ , __FILE__ , __LINE__ ,                                     \
          CPPA_CONCATL(cppa_trace_helper_) .str() CPPA_RPAREN
#  define CPPA_LOGF_TRACE(message)                                             \
    ::std::ostringstream CPPA_CONCATL(cppa_trace_helper_) ;                    \
    CPPA_CONCATL(cppa_trace_helper_) << message ;                              \
    ::cppa::detail::logging::trace_helper CPPA_CONCATL(cppa_fun_trace_helper_) \
        CPPA_LPAREN "NONE" ,                                                   \
          __func__ , __FILE__ , __LINE__ ,                                     \
          CPPA_CONCATL(cppa_trace_helper_) .str() CPPA_RPAREN

#else
#  define CPPA_LOG_TRACE(unused)
#  define CPPA_LOGF_TRACE(unused)
#endif

#define CPPA_LOG_ERROR_IF(stmt,message)                                        \
    if (stmt) { CPPA_LOG_ERROR(message); }; CPPA_VOID_STMT

#define CPPA_LOG_WARNING_IF(stmt,message)                                      \
    if (stmt) { CPPA_LOG_WARNING(message); }; CPPA_VOID_STMT

#define CPPA_LOG_INFO_IF(stmt,message)                                         \
    if (stmt) { CPPA_LOG_INFO(message); }; CPPA_VOID_STMT

#define CPPA_LOG_DEBUG_IF(stmt,message)                                        \
    if (stmt) { CPPA_LOG_DEBUG(message); }; CPPA_VOID_STMT

#define CPPA_LOGF_ERROR_IF(stmt,message)                                       \
    if (stmt) { CPPA_LOGF_ERROR(message); }; CPPA_VOID_STMT

#define CPPA_LOGF_WARNING_IF(stmt,message)                                     \
    if (stmt) { CPPA_LOGF_WARNING(message); }; CPPA_VOID_STMT

#define CPPA_LOGF_INFO_IF(stmt,message)                                        \
    if (stmt) { CPPA_LOGF_INFO(message); }; CPPA_VOID_STMT

#define CPPA_LOGF_DEBUG_IF(stmt,message)                                       \
    if (stmt) { CPPA_LOGF_DEBUG(message); }; CPPA_VOID_STMT


#endif // CPPA_DEBUG

#define CPPA_ARG(arg) #arg << " = " << arg
#define CPPA_TARG(arg, trans) #arg << " = " << trans ( arg )
#define CPPA_MARG(arg, memfun) #arg << " = " << arg . memfun ()

#endif // CPPA_LOGGING_HPP
