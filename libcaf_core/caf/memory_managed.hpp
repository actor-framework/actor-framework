/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_MEMORY_MANAGED_HPP
#define CAF_MEMORY_MANAGED_HPP

namespace caf {

/// This base enables derived classes to enforce a different
/// allocation strategy than new/delete by providing a virtual
/// protected `request_deletion()` function and non-public destructor.
class memory_managed {
public:
  /// Default implementations calls `delete this, but can
  /// be overriden in case deletion depends on some condition or
  /// the class doesn't use default new/delete.
  /// @param decremented_rc Indicates whether the caller did reduce the
  ///                       reference of this object before calling this member
  ///                       function. This information is important when
  ///                       implementing a type with support for weak pointers.
  virtual void request_deletion(bool decremented_rc) noexcept;

protected:
  virtual ~memory_managed();
};

} // namespace caf

#endif // CAF_MEMORY_MANAGED_HPP
