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
#include <execinfo.h>
#include <type_traits>

#include "cppa/self.hpp"
#include "cppa/actor.hpp"
#include "cppa/singletons.hpp"
#include "cppa/local_actor.hpp"
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
                     const actor_ptr& from,
                     const std::string& msg    ) = 0;

    class trace_helper {

     public:

        trace_helper(std::string class_name,
                     const char* fun_name,
                     const char* file_name,
                     int line_num,
                     actor_ptr aptr,
                     const std::string& msg);

        ~trace_helper();

     private:

        std::string m_class;
        const char* m_fun_name;
        const char* m_file_name;
        int         m_line_num;
        actor_ptr   m_self;

    };

 protected:

    virtual ~logging();

    static logging* create_singleton();

    virtual void initialize() = 0;

    virtual void destroy() = 0;

    inline void dispose() { delete this; }

};

inline actor_ptr fwd_aptr(const self_type& s) {
    return s.unchecked();
}

inline actor_ptr fwd_aptr(actor_ptr ptr) {
    return std::move(ptr);
}

struct oss_wr {

    inline oss_wr() { }
    inline oss_wr(oss_wr&& other) : m_str(std::move(other.m_str)) { }

    std::string m_str;

    inline std::string str() {
        return std::move(m_str);
    }

};

inline oss_wr operator<<(oss_wr&& lhs, std::string str) {
    lhs.m_str += std::move(str);
    return std::move(lhs);
}

inline oss_wr operator<<(oss_wr&& lhs, const char* str) {
    lhs.m_str += str;
    return std::move(lhs);
}

template<typename T>
oss_wr operator<<(oss_wr&& lhs, T rhs) {
    std::ostringstream oss;
    oss << rhs;
    lhs.m_str += oss.str();
    return std::move(lhs);
}

} // namespace cppa

#define CPPA_VOID_STMT static_cast<void>(0)

#define CPPA_CAT(a, b) a ## b

#define CPPA_ERROR    0
#define CPPA_WARNING  1
#define CPPA_INFO     2
#define CPPA_DEBUG    3
#define CPPA_TRACE    4

#ifdef CPPA_DEBUG_MODE
#   define CPPA_SET_DEBUG_NAME(strstr)                                            \
        self->debug_name((::cppa::oss_wr{} << strstr).str());
#else
#   define CPPA_SET_DEBUG_NAME(unused)
#endif

#define CPPA_LVL_NAME0() "ERROR"
#define CPPA_LVL_NAME1() "WARN "
#define CPPA_LVL_NAME2() "INFO "
#define CPPA_LVL_NAME3() "DEBUG"
#define CPPA_LVL_NAME4() "TRACE"

#ifndef CPPA_LOG_LEVEL
#   define CPPA_LOG_IMPL(lvlname, classname, funname, unused, message) {       \
        std::cerr << "[" << lvlname << "] " << classname << "::"               \
                  << funname << ": " << message << "\nStack trace:\n";         \
        void *array[10];                                                       \
        size_t size = backtrace(array, 10);                                    \
        backtrace_symbols_fd(array, size, 2);                                  \
    } CPPA_VOID_STMT
#   define CPPA_LOG_LEVEL 1
#else
#   define CPPA_LOG_IMPL(lvlname, classname, funname, aptr, message)           \
        ::cppa::get_logger()->log(lvlname, classname, funname, __FILE__,       \
            __LINE__, ::cppa::fwd_aptr(aptr),                                  \
            (::cppa::oss_wr{} << message).str())
#endif

#define CPPA_CLASS_NAME ::cppa::detail::demangle(typeid(*this)).c_str()

#define CPPA_PRINT0(lvlname, classname, funname, actorptr, msg)                \
    CPPA_LOG_IMPL(lvlname, classname, funname, actorptr, msg)

#define CPPA_PRINT_IF0(stmt, lvlname, classname, funname, actorptr, msg)       \
    if (stmt) { CPPA_LOG_IMPL(lvlname, classname, funname, actorptr, msg); }   \
    CPPA_VOID_STMT

#define CPPA_PRINT1(lvlname, classname, funname, actorptr, msg)                \
    CPPA_PRINT0(lvlname, classname, funname, actorptr, msg)

#define CPPA_PRINT_IF1(stmt, lvlname, classname, funname, actorptr, msg)       \
    CPPA_PRINT_IF0(stmt, lvlname, classname, funname, actorptr, msg)

#if CPPA_LOG_LEVEL < 4
#       define CPPA_PRINT4(arg0, arg1, arg2, arg3, arg4)
#   else
#       define CPPA_PRINT4(lvlname, classname, funname, actorptr, msg)         \
               ::cppa::logging::trace_helper cppa_trace_helper_ {              \
                   classname, funname, __FILE__, __LINE__,                     \
                   ::cppa::fwd_aptr(actorptr),                                 \
                   (::cppa::oss_wr{} << msg).str()                             \
               }
#endif

#if CPPA_LOG_LEVEL < 3
#       define CPPA_PRINT3(arg0, arg1, arg2, arg3, arg4)
#       define CPPA_PRINT_IF3(arg0, arg1, arg2, arg3, arg4, arg5)
#   else
#       define CPPA_PRINT3(lvlname, classname, funname, actorptr, msg)         \
               CPPA_PRINT0(lvlname, classname, funname, actorptr, msg)
#       define CPPA_PRINT_IF3(stmt, lvlname, classname, funname, actorptr, msg)\
               CPPA_PRINT_IF0(stmt, lvlname, classname, funname, actorptr, msg)
#endif

#if CPPA_LOG_LEVEL < 2
#       define CPPA_PRINT2(arg0, arg1, arg2, arg3, arg4)
#       define CPPA_PRINT_IF2(arg0, arg1, arg2, arg3, arg4, arg5)
#   else
#       define CPPA_PRINT2(lvlname, classname, funname, actorptr, msg)         \
               CPPA_PRINT0(lvlname, classname, funname, actorptr, msg)
#       define CPPA_PRINT_IF2(stmt, lvlname, classname, funname, actorptr, msg)\
               CPPA_PRINT_IF0(stmt, lvlname, classname, funname, actorptr, msg)
#endif

#define CPPA_EVAL(what) what

/**
 * @def CPPA_LOGC
 * @brief Logs a message with custom class and function names.
 **/
#define CPPA_LOGC(level, classname, funname, actorptr, msg)                    \
    CPPA_CAT(CPPA_PRINT, level)(CPPA_CAT(CPPA_LVL_NAME, level)(), classname,   \
                                funname, actorptr, msg)

/**
 * @def CPPA_LOGF
 * @brief Logs a message inside a free function.
 **/
#define CPPA_LOGF(level, actorptr, msg)                                        \
    CPPA_LOGC(level, "NONE", __func__, actorptr, msg)

/**
 * @def CPPA_LOGMF
 * @brief Logs a message inside a member function.
 **/
#define CPPA_LOGMF(level, actorptr, msg)                                       \
    CPPA_LOGC(level, CPPA_CLASS_NAME, __func__, actorptr, msg)

/**
 * @def CPPA_LOGC
 * @brief Logs a message with custom class and function names.
 **/
#define CPPA_LOGC_IF(stmt, level, classname, funname, actorptr, msg)           \
    CPPA_CAT(CPPA_PRINT_IF, level)(stmt, CPPA_CAT(CPPA_LVL_NAME, level)(),     \
                                   classname, funname, actorptr, msg)

/**
 * @def CPPA_LOGF
 * @brief Logs a message inside a free function.
 **/
#define CPPA_LOGF_IF(stmt, level, actorptr, msg)                               \
    CPPA_LOGC_IF(stmt, level, "NONE", __func__, actorptr, msg)

/**
 * @def CPPA_LOGMF
 * @brief Logs a message inside a member function.
 **/
#define CPPA_LOGMF_IF(stmt, level, actorptr, msg)                              \
    CPPA_LOGC_IF(stmt, level, CPPA_CLASS_NAME, __func__, actorptr, msg)

// convenience macros to safe some typing when printing arguments
#define CPPA_ARG(arg) #arg << " = " << arg
#define CPPA_TARG(arg, trans) #arg << " = " << trans ( arg )
#define CPPA_MARG(arg, memfun) #arg << " = " << arg . memfun ()
#define CPPA_TSARG(arg) #arg << " = " << to_string ( arg )


/******************************************************************************
 *                             convenience macros                             *
 ******************************************************************************/

#define CPPA_LOG_ERROR(msg)   CPPA_LOGMF(CPPA_ERROR, ::cppa::self, msg)
#define CPPA_LOG_WARNING(msg) CPPA_LOGMF(CPPA_WARNING,  ::cppa::self, msg)
#define CPPA_LOG_DEBUG(msg)   CPPA_LOGMF(CPPA_DEBUG, ::cppa::self, msg)
#define CPPA_LOG_INFO(msg)    CPPA_LOGMF(CPPA_INFO,  ::cppa::self, msg)
#define CPPA_LOG_TRACE(msg)   CPPA_LOGMF(CPPA_TRACE, ::cppa::self, msg)

#define CPPA_LOG_ERROR_IF(stmt, msg)   CPPA_LOGMF_IF(stmt, CPPA_ERROR, ::cppa::self, msg)
#define CPPA_LOG_WARNING_IF(stmt, msg) CPPA_LOGMF_IF(stmt, CPPA_WARNING,  ::cppa::self, msg)
#define CPPA_LOG_DEBUG_IF(stmt, msg)   CPPA_LOGMF_IF(stmt, CPPA_DEBUG, ::cppa::self, msg)
#define CPPA_LOG_INFO_IF(stmt, msg)    CPPA_LOGMF_IF(stmt, CPPA_INFO,  ::cppa::self, msg)
#define CPPA_LOG_TRACE_IF(stmt, msg)   CPPA_LOGMF_IF(stmt, CPPA_TRACE, ::cppa::self, msg)

#define CPPA_LOGC_ERROR(cname, fname, msg) \
        CPPA_LOGC(CPPA_ERROR, cname, fname, ::cppa::self, msg)
#define CPPA_LOGC_WARNING(cname, fname, msg) \
        CPPA_LOGC(CPPA_WARNING,  cname, fname, ::cppa::self, msg)
#define CPPA_LOGC_DEBUG(cname, fname, msg) \
        CPPA_LOGC(CPPA_DEBUG, cname, fname, ::cppa::self, msg)
#define CPPA_LOGC_INFO(cname, fname, msg) \
        CPPA_LOGC(CPPA_INFO,  cname, fname, ::cppa::self, msg)
#define CPPA_LOGC_TRACE(cname, fname, msg) \
        CPPA_LOGC(CPPA_TRACE, cname, fname, ::cppa::self, msg)

#define CPPA_LOGC_ERROR_IF(stmt, cname, fname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_ERROR, cname, fname, ::cppa::self, msg)
#define CPPA_LOGC_WARNING_IF(stmt, cname, fname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_WARNING,  cname, fname, ::cppa::self, msg)
#define CPPA_LOGC_DEBUG_IF(stmt, cname, fname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_DEBUG, cname, fname, ::cppa::self, msg)
#define CPPA_LOGC_INFO_IF(stmt, cname, fname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_INFO,  cname, fname, ::cppa::self, msg)
#define CPPA_LOGC_TRACE_IF(stmt, cname, fname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_TRACE, cname, fname, ::cppa::self, msg)

#define CPPA_LOGF_ERROR(msg)   CPPA_LOGF(CPPA_ERROR, ::cppa::self, msg)
#define CPPA_LOGF_WARNING(msg) CPPA_LOGF(CPPA_WARNING,  ::cppa::self, msg)
#define CPPA_LOGF_DEBUG(msg)   CPPA_LOGF(CPPA_DEBUG, ::cppa::self, msg)
#define CPPA_LOGF_INFO(msg)    CPPA_LOGF(CPPA_INFO,  ::cppa::self, msg)
#define CPPA_LOGF_TRACE(msg)   CPPA_LOGF(CPPA_TRACE, ::cppa::self, msg)

#define CPPA_LOGF_ERROR_IF(stmt, msg)   CPPA_LOGF_IF(stmt, CPPA_ERROR, ::cppa::self, msg)
#define CPPA_LOGF_WARNING_IF(stmt, msg) CPPA_LOGF_IF(stmt, CPPA_WARNING,  ::cppa::self, msg)
#define CPPA_LOGF_DEBUG_IF(stmt, msg)   CPPA_LOGF_IF(stmt, CPPA_DEBUG, ::cppa::self, msg)
#define CPPA_LOGF_INFO_IF(stmt, msg)    CPPA_LOGF_IF(stmt, CPPA_INFO,  ::cppa::self, msg)
#define CPPA_LOGF_TRACE_IF(stmt, msg)   CPPA_LOGF_IF(stmt, CPPA_TRACE, ::cppa::self, msg)

#define CPPA_LOGM_ERROR(cname, msg) \
        CPPA_LOGC(CPPA_ERROR, cname, __func__, ::cppa::self, msg)
#define CPPA_LOGM_WARNING(cname, msg) \
        CPPA_LOGC(CPPA_WARNING,  cname, ::cppa::self, msg)
#define CPPA_LOGM_DEBUG(cname, msg) \
        CPPA_LOGC(CPPA_DEBUG, cname, __func__, ::cppa::self, msg)
#define CPPA_LOGM_INFO(cname, msg) \
        CPPA_LOGC(CPPA_INFO,  cname, ::cppa::self, msg)
#define CPPA_LOGM_TRACE(cname, msg) \
        CPPA_LOGC(CPPA_TRACE, cname, __func__, ::cppa::self, msg)

#define CPPA_LOGM_ERROR_IF(stmt, cname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_ERROR, cname, __func__, ::cppa::self, msg)
#define CPPA_LOGM_WARNING_IF(stmt, cname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_WARNING,  cname, ::cppa::self, msg)
#define CPPA_LOGM_DEBUG_IF(stmt, cname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_DEBUG, cname, __func__, ::cppa::self, msg)
#define CPPA_LOGM_INFO_IF(stmt, cname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_INFO,  cname, ::cppa::self, msg)
#define CPPA_LOGM_TRACE_IF(stmt, cname, msg) \
        CPPA_LOGC_IF(stmt, CPPA_TRACE, cname, __func__, ::cppa::self, msg)

#endif // CPPA_LOGGING_HPP
