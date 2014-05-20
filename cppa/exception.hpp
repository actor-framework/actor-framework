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
 * Copyright (C) 2011-2014                                                    *
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


#ifndef CPPA_EXCEPTION_HPP
#define CPPA_EXCEPTION_HPP

#include <string>
#include <cstdint>
#include <exception>
#include <stdexcept>

namespace cppa {

/**
 * @brief Base class for libcppa exceptions.
 */
class cppa_exception : public std::exception {

 public:

    ~cppa_exception();

    cppa_exception() = delete;
    cppa_exception(const cppa_exception&) = default;
    cppa_exception& operator=(const cppa_exception&) = default;

    /**
     * @brief Returns the error message.
     * @returns The error message as C-string.
     */
    const char* what() const noexcept;

 protected:

    /**
     * @brief Creates an exception with the error string @p what_str.
     * @param what_str Error message as rvalue string.
     */
    cppa_exception(std::string&& what_str);

    /**
     * @brief Creates an exception with the error string @p what_str.
     * @param what_str Error message as string.
     */
    cppa_exception(const std::string& what_str);

 private:

    std::string m_what;

};

/**
 * @brief Thrown if an actor finished execution.
 */
class actor_exited : public cppa_exception {

 public:

    ~actor_exited();

    actor_exited(std::uint32_t exit_reason);

    actor_exited(const actor_exited&) = default;
    actor_exited& operator=(const actor_exited&) = default;

    /**
     * @brief Gets the exit reason.
     * @returns The exit reason of the terminating actor either set via
     *          {@link quit} or by a special (exit) message.
     */
    inline std::uint32_t reason() const noexcept;

 private:

    std::uint32_t m_reason;

};

/**
 * @brief Thrown to indicate that either an actor publishing failed or
 *        @p libcppa was unable to connect to a remote host.
 */
class network_error : public cppa_exception {

 public:

    ~network_error();
    network_error(std::string&& what_str);
    network_error(const std::string& what_str);
    network_error(const network_error&) = default;
    network_error& operator=(const network_error&) = default;

};

/**
 * @brief Thrown to indicate that an actor publishing failed because
 *        the requested port could not be used.
 */
class bind_failure : public network_error {

 public:

    ~bind_failure();

    bind_failure(int bind_errno);
    bind_failure(const bind_failure&) = default;
    bind_failure& operator=(const bind_failure&) = default;

    /**
     * @brief Gets the socket API error code.
     * @returns The errno set by <tt>bind()</tt>.
     */
    inline int error_code() const noexcept;

 private:

    int m_errno;

};

inline std::uint32_t actor_exited::reason() const noexcept {
    return m_reason;
}

inline int bind_failure::error_code() const noexcept {
    return m_errno;
}

} // namespace cppa

#endif // CPPA_EXCEPTION_HPP
