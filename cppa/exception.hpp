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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
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

/**
 * @brief Thrown to indicate that a client tried to read from
 *        a stream after is has been shutdown by the remote side.
 */
class stream_at_eof : public network_error {

    using super = network_error;

 public:

    ~stream_at_eof();

    template<typename... Ts>
    stream_at_eof(Ts&&... args) : super(std::forward<Ts>(args)...) { }

};

inline std::uint32_t actor_exited::reason() const noexcept {
    return m_reason;
}

inline int bind_failure::error_code() const noexcept {
    return m_errno;
}

} // namespace cppa

#endif // CPPA_EXCEPTION_HPP
