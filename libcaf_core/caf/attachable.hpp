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
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
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
#include <exception>

#include "caf/optional.hpp"

namespace caf {

class abstract_actor;

/**
 * Callback utility class.
 */
class attachable {
 public:
  attachable() = default;
  attachable(const attachable&) = delete;
  attachable& operator=(const attachable&) = delete;

  /**
   * Represents a pointer to a value with its RTTI.
   */
  struct token {
    template <class T>
    token(const T& tk) : subtype(typeid(T)), ptr(&tk) {
      // nop
    }

    /**
     * Denotes the type of ptr.
     */
    const std::type_info& subtype;

    /**
     * Any value, used to identify attachable instances.
     */
    const void* ptr;

    token(const std::type_info& subtype, const void* ptr);
  };

  virtual ~attachable();

  /**
   * Executed if the actor did not handle an exception and must
   * not return `none` if this attachable did handle `eptr`.
   * Note that the first handler to handle `eptr` "wins" and no other
   * handler will be invoked.
   * @returns The exit reason the actor should use.
   */
  virtual optional<uint32_t> handle_exception(const std::exception_ptr& eptr);

  /**
   * Executed if the actor finished execution with given `reason`.
   * The default implementation does nothing.
   */
  virtual void actor_exited(abstract_actor* self, uint32_t reason);

  /**
   * Returns `true` if `what` selects this instance, otherwise `false`.
   */
  virtual bool matches(const token& what);

  /**
   * Returns `true` if `what` selects this instance, otherwise `false`.
   */
  template <class T>
  bool matches(const T& what) {
    return matches(token{typeid(T), &what});
  }

  std::unique_ptr<attachable> next;
};

/**
 * @relates attachable
 */
using attachable_ptr = std::unique_ptr<attachable>;

} // namespace caf

#endif // CAF_ATTACHABLE_HPP
