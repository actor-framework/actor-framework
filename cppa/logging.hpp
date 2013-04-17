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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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
#include <iostream>

#include "cppa/singletons.hpp"
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

    class trace_helper {

     public:

        trace_helper(std::string class_name,
                     const char* fun_name,
                     const char* file_name,
                     int line_num,
                     const std::string& msg);

        ~trace_helper();

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

#define CPPA_LIF(stmt, logstmt) if (stmt) { logstmt ; } CPPA_VOID_STMT

#ifndef CPPA_LOG_LEVEL
#   define CPPA_LOG(classname, funname, level, message)                        \
        std::cerr << level << " [" << classname << "::" << funname << "]: "    \
                  << message << std::endl;
#else
#   define CPPA_LOG(classname, funname, level, message) {                      \
        std::ostringstream scoped_oss; scoped_oss << message;                  \
        ::cppa::get_logger()->log(                                             \
            level, classname, funname, __FILE__, __LINE__, scoped_oss.str());  \
    } CPPA_VOID_STMT
#endif

#define CPPA_CLASS_NAME ::cppa::detail::demangle(typeid(*this)).c_str()

// errors and warnings are enabled by default

/**
 * @brief Logs a custom error message @p msg with class name @p cname
 *        and function name @p fname.
 */
#define CPPA_LOGC_ERROR(cname, fname, msg) CPPA_LOG(cname, fname, "ERROR", msg)
#define CPPA_LOGC_WARNING(cname, fname, msg) CPPA_LOG(cname, fname, "WARN ", msg)
#define CPPA_LOGC_INFO(cname, fname, msg) CPPA_VOID_STMT
#define CPPA_LOGC_DEBUG(cname, fname, msg) CPPA_VOID_STMT
#define CPPA_LOGC_TRACE(cname, fname, msg) CPPA_VOID_STMT

// enable info messages
#if CPPA_LOG_LEVEL > 1
#   undef CPPA_LOGC_INFO
#   define CPPA_LOGC_INFO(cname, fname, msg) CPPA_LOG(cname, fname, "INFO ", msg)
#   define CPPA_LOG_INFO_IF(stmt,msg) CPPA_LIF((stmt), CPPA_LOG_INFO(msg))
#   define CPPA_LOGF_INFO_IF(stmt,msg) CPPA_LIF((stmt), CPPA_LOGF_INFO(msg))
#else
#   define CPPA_LOG_INFO_IF(unused1,unused2) CPPA_VOID_STMT
#   define CPPA_LOGF_INFO_IF(unused1,unused2) CPPA_VOID_STMT
#endif

// enable debug messages
#if CPPA_LOG_LEVEL > 2
#   undef CPPA_LOGC_DEBUG
#   define CPPA_LOGC_DEBUG(cname, fname, msg) CPPA_LOG(cname, fname, "DEBUG", msg)
#   define CPPA_LOG_DEBUG_IF(stmt,msg) CPPA_LIF((stmt), CPPA_LOG_DEBUG(msg))
#   define CPPA_LOGF_DEBUG_IF(stmt,msg) CPPA_LIF((stmt), CPPA_LOGF_DEBUG(msg))
#else
#   define CPPA_LOG_DEBUG_IF(unused1,unused2) CPPA_VOID_STMT
#   define CPPA_LOGF_DEBUG_IF(unused1,unused2) CPPA_VOID_STMT
#endif

// enable trace messages
#if CPPA_LOG_LEVEL > 3
#   undef CPPA_LOGC_TRACE
#   define CPPA_CONCAT_I(lhs,rhs) lhs ## rhs
#   define CPPA_CONCAT(lhs,rhs) CPPA_CONCAT_I(lhs,rhs)
#   define CPPA_CONCATL(lhs) CPPA_CONCAT(lhs, __LINE__)
#   define CPPA_LOGC_TRACE(cname, fname, msg)                                  \
        ::std::ostringstream CPPA_CONCATL(cppa_trace_helper_) ;                \
        CPPA_CONCATL(cppa_trace_helper_) << msg ;                              \
        ::cppa::logging::trace_helper CPPA_CONCATL(cppa_fun_trace_helper_) {   \
            cname, fname , __FILE__ , __LINE__ ,                               \
            CPPA_CONCATL(cppa_trace_helper_) .str() }
#endif

/**
 * @brief Logs @p msg with custom member function @p fname.
 */
#define CPPA_LOGS_ERROR(fname, msg) CPPA_LOGC_ERROR(CPPA_CLASS_NAME, fname, msg)

/**
 * @brief Logs @p msg with custom class name @p cname.
 */
#define CPPA_LOGM_ERROR(cname, msg) CPPA_LOGC_ERROR(cname, __FUNCTION__, msg)

/**
 * @brief Logs @p msg in a free function if @p stmt evaluates to @p true.
 */
#define CPPA_LOGF_ERROR_IF(stmt, msg) CPPA_LIF((stmt), CPPA_LOGF_ERROR(msg))

/**
 * @brief Logs @p msg in a free function.
 */
#define CPPA_LOGF_ERROR(msg) CPPA_LOGM_ERROR("NONE", msg)

/**
 * @brief Logs @p msg in a member function if @p stmt evaluates to @p true.
 */
#define CPPA_LOG_ERROR_IF(stmt, msg) CPPA_LIF((stmt), CPPA_LOG_ERROR(msg))

/**
 * @brief Logs @p msg in a member function.
 */
#define CPPA_LOG_ERROR(msg) CPPA_LOGM_ERROR(CPPA_CLASS_NAME, msg)

// convenience macros for warnings
#define CPPA_LOG_WARNING(msg) CPPA_LOGM_WARNING(CPPA_CLASS_NAME, msg)
#define CPPA_LOGF_WARNING(msg) CPPA_LOGM_WARNING("NONE", msg)
#define CPPA_LOGM_WARNING(cname, msg) CPPA_LOGC_WARNING(cname, __FUNCTION__, msg)
#define CPPA_LOG_WARNING_IF(stmt,msg) CPPA_LIF((stmt), CPPA_LOG_WARNING(msg))
#define CPPA_LOGF_WARNING_IF(stmt,msg) CPPA_LIF((stmt), CPPA_LOGF_WARNING(msg))

// convenience macros for info messages
#define CPPA_LOG_INFO(msg) CPPA_LOGM_INFO(CPPA_CLASS_NAME, msg)
#define CPPA_LOGF_INFO(msg) CPPA_LOGM_INFO("NONE", msg)
#define CPPA_LOGM_INFO(cname, msg) CPPA_LOGC_INFO(cname, __FUNCTION__, msg)

// convenience macros for debug messages
#define CPPA_LOG_DEBUG(msg) CPPA_LOGM_DEBUG(CPPA_CLASS_NAME, msg)
#define CPPA_LOGF_DEBUG(msg) CPPA_LOGM_DEBUG("NONE", msg)
#define CPPA_LOGM_DEBUG(cname, msg) CPPA_LOGC_DEBUG(cname, __FUNCTION__, msg)

// convenience macros for trace messages
#define CPPA_LOGS_TRACE(fname, msg) CPPA_LOGC_TRACE(CPPA_CLASS_NAME, fname, msg)
#define CPPA_LOGM_TRACE(cname, msg) CPPA_LOGC_TRACE(cname, __FUNCTION__, msg)
#define CPPA_LOGF_TRACE(msg) CPPA_LOGM_TRACE("NONE", msg)
#define CPPA_LOG_TRACE(msg) CPPA_LOGM_TRACE(CPPA_CLASS_NAME, msg)

// convenience macros to safe some typing when printing arguments
#define CPPA_ARG(arg) #arg << " = " << arg
#define CPPA_TARG(arg, trans) #arg << " = " << trans ( arg )
#define CPPA_MARG(arg, memfun) #arg << " = " << arg . memfun ()

#endif // CPPA_LOGGING_HPP
