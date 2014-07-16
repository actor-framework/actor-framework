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

#ifndef CAF_EXCEPTION_HPP
#define CAF_EXCEPTION_HPP

#include <string>
#include <cstdint>
#include <exception>
#include <stdexcept>

namespace caf {

/**
 * @brief Base class for exceptions.
 */
class caf_exception : public std::exception {

 public:

    ~caf_exception();

    caf_exception() = delete;
    caf_exception(const caf_exception&) = default;
    caf_exception& operator=(const caf_exception&) = default;

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
    caf_exception(std::string&& what_str);

    /**
     * @brief Creates an exception with the error string @p what_str.
     * @param what_str Error message as string.
     */
    caf_exception(const std::string& what_str);

 private:

    std::string m_what;

};

/**
 * @brief Thrown if an actor finished execution.
 */
class actor_exited : public caf_exception {

 public:

    ~actor_exited();

    actor_exited(uint32_t exit_reason);

    actor_exited(const actor_exited&) = default;
    actor_exited& operator=(const actor_exited&) = default;

    /**
     * @brief Gets the exit reason.
     * @returns The exit reason of the terminating actor either set via
     *          {@link quit} or by a special (exit) message.
     */
    inline uint32_t reason() const noexcept;

 private:

    uint32_t m_reason;

};

/**
 * @brief Thrown to indicate that either an actor publishing failed or
 *        the middleman was unable to connect to a remote host.
 */
class network_error : public caf_exception {

    using super = caf_exception;

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

    using super = network_error;

 public:

    ~bind_failure();

    bind_failure(std::string&& what_str);
    bind_failure(const std::string& what_str);
    bind_failure(const bind_failure&) = default;
    bind_failure& operator=(const bind_failure&) = default;

};

inline uint32_t actor_exited::reason() const noexcept {
    return m_reason;
}

} // namespace caf

#endif // CAF_EXCEPTION_HPP
