/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_ATTACHABLE_HPP
#define CAF_ATTACHABLE_HPP

#include <memory>
#include <cstdint>
#include <typeinfo>

namespace caf {

/**
 * @brief Callback utility class.
 */
class attachable {

  attachable(const attachable&) = delete;
  attachable& operator=(const attachable&) = delete;

 protected:

  attachable() = default;

 public:

  /**
   * @brief Represents a pointer to a value with its RTTI.
   */
  struct token {

    /**
     * @brief Denotes the type of @c ptr.
     */
    const std::type_info& subtype;

    /**
     * @brief Any value, used to identify @c attachable instances.
     */
    const void* ptr;

    token(const std::type_info& subtype, const void* ptr);

  };

  virtual ~attachable();

  /**
   * @brief Executed if the actor finished execution with given @p reason.
   *
   * The default implementation does nothing.
   * @param reason The exit rason of the observed actor.
   */
  virtual void actor_exited(uint32_t reason) = 0;

  /**
   * @brief Selects a group of @c attachable instances by @p what.
   * @param what A value that selects zero or more @c attachable instances.
   * @returns @c true if @p what selects this instance; otherwise @c false.
   */
  virtual bool matches(const token& what) = 0;

};

/**
 * @brief A managed {@link attachable} pointer.
 * @relates attachable
 */
using attachable_ptr = std::unique_ptr<attachable>;

} // namespace caf

#endif // CAF_ATTACHABLE_HPP
