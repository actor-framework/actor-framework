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

#ifndef CAF_ABSTRACT_ACTOR_HPP
#define CAF_ABSTRACT_ACTOR_HPP

#include <set>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <type_traits>

#include "caf/node_id.hpp"
#include "caf/fwd.hpp"
#include "caf/attachable.hpp"
#include "caf/message_id.hpp"
#include "caf/exit_reason.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/abstract_channel.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

class actor_addr;
class serializer;
class deserializer;
class execution_unit;

/**
 * @brief A unique actor ID.
 * @relates abstract_actor
 */
using actor_id = uint32_t;

/**
 * @brief Denotes an ID that is never used by an actor.
 */
constexpr actor_id invalid_actor_id = 0;

class actor;
class abstract_actor;
class response_promise;

using abstract_actor_ptr = intrusive_ptr<abstract_actor>;

/**
 * @brief Base class for all actor implementations.
 */
class abstract_actor : public abstract_channel {

  friend class response_promise;

  using super = abstract_channel;

 public:

  /**
   * @brief Attaches @p ptr to this actor.
   *
   * The actor will call <tt>ptr->detach(...)</tt> on exit, or immediately
   * if it already finished execution.
   * @param ptr A callback object that's executed if the actor finishes
   *      execution.
   * @returns @c true if @p ptr was successfully attached to the actor;
   *      otherwise (actor already exited) @p false.
   * @warning The actor takes ownership of @p ptr.
   */
  bool attach(attachable_ptr ptr);

  /**
   * @brief Convenience function that attaches the functor
   *    or function @p f to this actor.
   *
   * The actor executes <tt>f()</tt> on exit, or immediatley
   * if it already finished execution.
   * @param f A functor, function or lambda expression that's executed
   *       if the actor finishes execution.
   * @returns @c true if @p f was successfully attached to the actor;
   *      otherwise (actor already exited) @p false.
   */
  template <class F>
  bool attach_functor(F&& f);

  /**
   * @brief Returns the address of this actor.
   */
  actor_addr address() const;

  /**
   * @brief Detaches the first attached object that matches @p what.
   */
  void detach(const attachable::token& what);

  /**
   * @brief Links this actor to @p whom.
   * @param whom Actor instance that whose execution is coupled to the
   *       execution of this Actor.
   */
  virtual void link_to(const actor_addr& whom);

  /**
   * @brief Links this actor to @p whom.
   * @param whom Actor instance that whose execution is coupled to the
   *       execution of this Actor.
   */
  template <class ActorHandle>
  void link_to(const ActorHandle& whom) {
    link_to(whom.address());
  }

  /**
   * @brief Unlinks this actor from @p whom.
   * @param whom Linked Actor.
   * @note Links are automatically removed if the actor finishes execution.
   */
  virtual void unlink_from(const actor_addr& whom);

  /**
   * @brief Unlinks this actor from @p whom.
   * @param whom Linked Actor.
   * @note Links are automatically removed if the actor finishes execution.
   */
  template <class ActorHandle>
  void unlink_from(const ActorHandle& whom) {
    unlink_from(whom.address());
  }

  /**
   * @brief Establishes a link relation between this actor and @p other.
   * @param other Actor instance that wants to link against this Actor.
   * @returns @c true if this actor is running and added @p other to its
   *      list of linked actors; otherwise @c false.
   */
  virtual bool establish_backlink(const actor_addr& other);

  /**
   * @brief Removes a link relation between this actor and @p other.
   * @param other Actor instance that wants to unlink from this Actor.
   * @returns @c true if this actor is running and removed @p other
   *      from its list of linked actors; otherwise @c false.
   */
  virtual bool remove_backlink(const actor_addr& other);

  /**
   * @brief Gets an integer value that uniquely identifies this Actor in
   *    the process it's executed in.
   * @returns The unique identifier of this actor.
   */
  inline actor_id id() const;

  /**
   * @brief Returns the actor's exit reason of
   *    <tt>exit_reason::not_exited</tt> if it's still alive.
   */
  inline uint32_t exit_reason() const;

  /**
   * @brief Returns the type interface as set of strings.
   * @note The returned set is empty for all untyped actors.
   */
  virtual std::set<std::string> interface() const;

 protected:

  abstract_actor();

  abstract_actor(actor_id aid, node_id nid);

  /**
   * @brief Should be overridden by subtypes and called upon termination.
   * @note Default implementation sets 'exit_reason' accordingly.
   * @note Overridden functions should always call super::cleanup().
   */
  virtual void cleanup(uint32_t reason);

  /**
   * @brief The default implementation for {@link link_to()}.
   */
  bool link_to_impl(const actor_addr& other);

  /**
   * @brief The default implementation for {@link unlink_from()}.
   */
  bool unlink_from_impl(const actor_addr& other);

  /**
   * @brief Returns <tt>exit_reason() != exit_reason::not_exited</tt>.
   */
  inline bool exited() const;

  // cannot be changed after construction
  const actor_id m_id;

  // you're either a proxy or you're not
  const bool m_is_proxy;

 private:

  // initially exit_reason::not_exited
  std::atomic<uint32_t> m_exit_reason;

  // guards access to m_exit_reason, m_attachables, and m_links
  std::mutex m_mtx;

  // links to other actors
  std::vector<abstract_actor_ptr> m_links;

  // attached functors that are executed on cleanup
  std::vector<attachable_ptr> m_attachables;

 protected:

  // identifies the execution unit this actor is currently executed by
  execution_unit* m_host;

};

/******************************************************************************
 *       inline and template member function implementations      *
 ******************************************************************************/

inline uint32_t abstract_actor::id() const { return m_id; }

inline uint32_t abstract_actor::exit_reason() const { return m_exit_reason; }

inline bool abstract_actor::exited() const {
  return exit_reason() != exit_reason::not_exited;
}

template <class F>
struct functor_attachable : attachable {

  F m_functor;

  template <class T>
  inline functor_attachable(T&& arg) : m_functor(std::forward<T>(arg)) { }

  void actor_exited(uint32_t reason) { m_functor(reason); }

  bool matches(const attachable::token&) { return false; }

};

template <class F>
bool abstract_actor::attach_functor(F&& f) {
  using f_type = typename detail::rm_const_and_ref<F>::type;
  using impl = functor_attachable<f_type>;
  return attach(attachable_ptr{new impl(std::forward<F>(f))});
}

} // namespace caf

#endif // CAF_ABSTRACT_ACTOR_HPP
