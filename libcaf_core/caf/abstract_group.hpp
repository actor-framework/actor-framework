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

#ifndef CAF_ABSTRACT_GROUP_HPP
#define CAF_ABSTRACT_GROUP_HPP

#include <string>
#include <memory>

#include "caf/channel.hpp"
#include "caf/ref_counted.hpp"
#include "caf/abstract_channel.hpp"

namespace caf {
namespace detail {

class group_manager;
class peer_connection;

} // namespace detail
} // namespace caf

namespace caf {

class group;
class serializer;
class deserializer;

/**
 * A multicast group.
 */
class abstract_group : public abstract_channel {
 public:
  friend class detail::group_manager;
  friend class detail::peer_connection;

  ~abstract_group();

  class subscription;

  // needs access to unsubscribe()
  friend class subscription;

  // unsubscribes its channel from the group on destruction
  class subscription {
   public:
    friend class abstract_group;
    subscription(const subscription&) = delete;
    subscription& operator=(const subscription&) = delete;
    subscription() = default;
    subscription(subscription&&) = default;
    subscription(const channel& s, const intrusive_ptr<abstract_group>& g);

    ~subscription();

    inline bool valid() const {
      return (m_subscriber) && (m_group);
    }

   private:
    channel m_subscriber;
    intrusive_ptr<abstract_group> m_group;
  };

  /**
   * Interface for user-defined multicast implementations.
   */
  class module {
   public:
    module(std::string module_name);

    virtual ~module();

    /**
     * Returns the name of this module implementation.
     * @threadsafe
     */
    const std::string& name();

    /**
     * Returns a pointer to the group associated with the name `group_name`.
     * @threadsafe
     */
    virtual group get(const std::string& group_name) = 0;

    virtual group deserialize(deserializer* source) = 0;

   private:
    std::string m_name;
  };

  using module_ptr = module*;
  using unique_module_ptr = std::unique_ptr<module>;

  virtual void serialize(serializer* sink) = 0;

  /**
   * Returns a string representation of the group identifier, e.g.,
   * "224.0.0.1" for IPv4 multicast or a user-defined string for local groups.
   */
  const std::string& identifier() const;

  module_ptr get_module() const;

  /**
   * Returns the name of the module.
   */
  const std::string& module_name() const;

  /**
   * Subscribes `who` to this group and returns a subscription object.
   */
  virtual subscription subscribe(const channel& who) = 0;

 protected:
  abstract_group(module_ptr module, std::string group_id);
  // called by subscription objects
  virtual void unsubscribe(const channel& who) = 0;
  module_ptr m_module;
  std::string m_identifier;
};

/**
 * A smart pointer type that manages instances of {@link group}.
 * @relates group
 */
using abstract_group_ptr = intrusive_ptr<abstract_group>;

} // namespace caf

#endif // CAF_ABSTRACT_GROUP_HPP
