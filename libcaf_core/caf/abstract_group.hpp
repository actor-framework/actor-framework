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

#ifndef CAF_ABSTRACT_GROUP_HPP
#define CAF_ABSTRACT_GROUP_HPP

#include <string>
#include <memory>

#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
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
class local_actor;
class deserializer;

/// A multicast group.
class abstract_group : public abstract_channel {
public:
  friend class detail::group_manager;
  friend class detail::peer_connection;
  friend class local_actor;

  ~abstract_group();

  class subscription;

  struct subscription_token {
    intrusive_ptr<abstract_group> group;
    static constexpr size_t token_type = attachable::token::subscription;
  };

  class subscription_predicate {
  public:
    inline subscription_predicate(intrusive_ptr<abstract_group> group)
        : group_(std::move(group)) {
      // nop
    }
    inline bool operator()(const attachable_ptr& ptr) {
      return ptr->matches(subscription_token{group_});
    }
  private:
    intrusive_ptr<abstract_group> group_;
  };

  // needs access to unsubscribe()
  friend class subscription;

  // unsubscribes its channel from the group on destruction
  class subscription : public attachable {
  public:
    subscription(const intrusive_ptr<abstract_group>& g);

    void actor_exited(abstract_actor* self, uint32_t reason) override;

    bool matches(const token& what) override;

    static inline attachable_ptr make(intrusive_ptr<abstract_group> ptr) {
      return attachable_ptr{new subscription(std::move(ptr))};
    }

    const intrusive_ptr<abstract_group>& group() const {
      return group_;
    }

  private:
    intrusive_ptr<abstract_group> group_;
  };

  /// Interface for user-defined multicast implementations.
  class module {
  public:
    module(std::string module_name);

    virtual ~module();

    /// Stops all groups from this module.
    virtual void stop() = 0;

    /// Returns the name of this module implementation.
    /// @threadsafe
    const std::string& name() const;

    /// Returns a pointer to the group associated with the name `group_name`.
    /// @threadsafe
    virtual group get(const std::string& group_name) = 0;

    virtual group deserialize(deserializer* source) = 0;

  private:
    std::string name_;
  };

  using module_ptr = module*;
  using unique_module_ptr = std::unique_ptr<module>;

  virtual void serialize(serializer* sink) = 0;

  /// Returns a string representation of the group identifier, e.g.,
  /// "224.0.0.1" for IPv4 multicast or a user-defined string for local groups.
  const std::string& identifier() const;

  module_ptr get_module() const;

  /// Returns the name of the module.
  const std::string& module_name() const;

  /// Subscribes `who` to this group and returns a subscription object.
  virtual attachable_ptr subscribe(const actor_addr& who) = 0;

  /// Stops any background actors or threads and IO handles.
  virtual void stop() = 0;

protected:
  abstract_group(module_ptr module, std::string group_id);
  // called by subscription objects
  virtual void unsubscribe(const actor_addr& who) = 0;
  module_ptr module_;
  std::string identifier_;
};

/// A smart pointer type that manages instances of {@link group}.
/// @relates group
using abstract_group_ptr = intrusive_ptr<abstract_group>;

} // namespace caf

#endif // CAF_ABSTRACT_GROUP_HPP
