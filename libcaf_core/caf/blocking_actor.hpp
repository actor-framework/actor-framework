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

#ifndef CAF_BLOCKING_ACTOR_HPP
#define CAF_BLOCKING_ACTOR_HPP

#include <mutex>
#include <condition_variable>

#include "caf/none.hpp"

#include "caf/on.hpp"
#include "caf/extend.hpp"
#include "caf/behavior.hpp"
#include "caf/local_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/response_handle.hpp"

#include "caf/detail/type_traits.hpp"

#include "caf/mixin/sync_sender.hpp"

namespace caf {

/// A thread-mapped or context-switching actor using a blocking
/// receive rather than a behavior-stack based message processing.
/// @extends local_actor
class blocking_actor
    : public extend<local_actor, blocking_actor>::
             with<mixin::sync_sender<blocking_response_handle_tag>::impl> {
public:
  blocking_actor();

  ~blocking_actor();

  /**************************************************************************
   *           utility stuff and receive() member function family           *
   **************************************************************************/

  using timeout_type = std::chrono::high_resolution_clock::time_point;

  struct receive_while_helper {
    std::function<void(behavior&)> dq_;
    std::function<bool()> stmt_;

    template <class... Ts>
    void operator()(Ts&&... xs) {
      static_assert(sizeof...(Ts) > 0,
              "operator() requires at least one argument");
      behavior bhvr{std::forward<Ts>(xs)...};
      while (stmt_()) dq_(bhvr);
    }
  };

  template <class T>
  struct receive_for_helper {
    std::function<void(behavior&)> dq_;
    T& begin;
    T end;

    template <class... Ts>
    void operator()(Ts&&... xs) {
      behavior bhvr{std::forward<Ts>(xs)...};
      for (; begin != end; ++begin) dq_(bhvr);
    }
  };

  struct do_receive_helper {
    std::function<void(behavior&)> dq_;
    behavior bhvr_;

    template <class Statement>
    void until(Statement stmt) {
      do {
        dq_(bhvr_);
      } while (stmt() == false);
    }

    void until(const bool& bvalue) {
      until([&] { return bvalue; });
    }
  };

  /// Dequeues the next message from the mailbox that is
  /// matched by given behavior.
  template <class... Ts>
  void receive(Ts&&... xs) {
    static_assert(sizeof...(Ts), "at least one argument required");
    behavior bhvr{std::forward<Ts>(xs)...};
    dequeue(bhvr);
  }

  /// Semantically equal to: `for (;;) { receive(...); }`, but does
  /// not cause a temporary behavior object per iteration.
  template <class... Ts>
  void receive_loop(Ts&&... xs) {
    behavior bhvr{std::forward<Ts>(xs)...};
    for (;;) dequeue(bhvr);
  }

  /// Receives messages for range `[begin, first)`.
  /// Semantically equal to:
  /// `for ( ; begin != end; ++begin) { receive(...); }`.
  ///
  /// **Usage example:**
  /// ~~~
  /// int i = 0;
  /// receive_for(i, 10) (
  ///   [&](get_atom) {
  ///     return i;
  ///   }
  /// );
  /// ~~~
  template <class T>
  receive_for_helper<T> receive_for(T& begin, const T& end) {
    return {make_dequeue_callback(), begin, end};
  }

  /// Receives messages as long as `stmt` returns true.
  /// Semantically equal to: `while (stmt()) { receive(...); }`.
  ///
  /// **Usage example:**
  /// ~~~
  /// int i = 0;
  /// receive_while([&]() { return (++i <= 10); })
  /// (
  ///   on<int>() >> int_fun,
  ///   on<float>() >> float_fun
  /// );
  /// ~~~
  template <class Statement>
  receive_while_helper receive_while(Statement stmt) {
    static_assert(std::is_same<bool, decltype(stmt())>::value,
            "functor or function does not return a boolean");
    return {make_dequeue_callback(), stmt};
  }

  /// Receives messages until `stmt` returns true.
  ///
  /// Semantically equal to:
  /// `do { receive(...); } while (stmt() == false);`
  ///
  /// **Usage example:**
  /// ~~~
  /// int i = 0;
  /// do_receive
  /// (
  ///   on<int>() >> int_fun,
  ///   on<float>() >> float_fun
  /// )
  /// .until([&]() { return (++i >= 10); };
  /// ~~~
  template <class... Ts>
  do_receive_helper do_receive(Ts&&... xs) {
    return {make_dequeue_callback(), behavior{std::forward<Ts>(xs)...}};
  }

  /// Blocks this actor until all other actors are done.
  void await_all_other_actors_done();

  /// Implements the actor's behavior.
  virtual void act();

  /// @cond PRIVATE

  void initialize() override;

  void dequeue(behavior& bhvr, message_id mid = invalid_message_id);

  /// @endcond

protected:
  // helper function to implement receive_(for|while) and do_receive
  std::function<void(behavior&)> make_dequeue_callback() {
    return [=](behavior& bhvr) { dequeue(bhvr); };
  }
};

} // namespace caf

#endif // CAF_BLOCKING_ACTOR_HPP
