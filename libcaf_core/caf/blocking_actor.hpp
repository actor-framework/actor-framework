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

#include <chrono>
#include <mutex>
#include <condition_variable>

#include "caf/fwd.hpp"
#include "caf/send.hpp"
#include "caf/none.hpp"
#include "caf/extend.hpp"
#include "caf/behavior.hpp"
#include "caf/local_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/actor_config.hpp"
#include "caf/actor_marker.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/is_timeout_or_catch_all.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/blocking_behavior.hpp"

#include "caf/mixin/sender.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/subscriber.hpp"

namespace caf {
namespace mixin {

template <>
struct is_blocking_requester<blocking_actor> : std::true_type { };

} // namespace caf
} // namespace mixin

namespace caf {

/// A thread-mapped or context-switching actor using a blocking
/// receive rather than a behavior-stack based message processing.
/// @extends local_actor
class blocking_actor
    : public extend<local_actor, blocking_actor>::
             with<mixin::requester,
                  mixin::sender,
                  mixin::subscriber>,
      public dynamically_typed_actor_base {
public:
  // -- member types -----------------------------------------------------------

  /// Absolute timeout type.
  using timeout_type = std::chrono::high_resolution_clock::time_point;

  /// Supported behavior type.
  using behavior_type = behavior;

  /// Declared message passing interface.
  using signatures = none_t;

  // -- nested classes ---------------------------------------------------------

  /// Represents pre- and postconditions for receive loops.
  class receive_cond {
  public:
    virtual ~receive_cond();

    /// Returns whether a precondition for receiving a message still holds.
    virtual bool pre();

    /// Returns whether a postcondition for receiving a message still holds.
    virtual bool post();
  };

  /// Pseudo receive condition modeling a single receive.
  class accept_one_cond : public receive_cond {
  public:
    virtual ~accept_one_cond();
    bool post() override;
  };

  /// Implementation helper for `blocking_actor::receive_while`.
  struct receive_while_helper {
    using fun_type = std::function<bool()>;

    blocking_actor* self;
    fun_type stmt_;

    template <class... Ts>
    void operator()(Ts&&... xs) {
      static_assert(sizeof...(Ts) > 0,
              "operator() requires at least one argument");
      struct cond : receive_cond {
        fun_type stmt;
        cond(fun_type x) : stmt(std::move(x)) {
          // nop
        }
        bool pre() override {
          return stmt();
        }
      };
      cond rc{std::move(stmt_)};
      self->varargs_receive(rc, message_id::make(), std::forward<Ts>(xs)...);
    }
  };

  /// Implementation helper for `blocking_actor::receive_for`.
  template <class T>
  struct receive_for_helper {
    blocking_actor* self;
    T& begin;
    T end;

    template <class... Ts>
    void operator()(Ts&&... xs) {
      struct cond : receive_cond {
        receive_for_helper& outer;
        cond(receive_for_helper& x) : outer(x) {
          // nop
        }
        bool pre() override {
          return outer.begin != outer.end;
        }
        bool post() override {
          ++outer.begin;
          return true;
        }
      };
      cond rc{*this};
      self->varargs_receive(rc, message_id::make(), std::forward<Ts>(xs)...);
    }
  };

  /// Implementation helper for `blocking_actor::do_receive`.
  struct do_receive_helper {
    std::function<void (receive_cond& rc)> cb;

    template <class Statement>
    void until(Statement stmt) {
      struct cond : receive_cond {
        Statement f;
        cond(Statement x) : f(std::move(x)) {
          // nop
        }
        bool post() override {
          return ! f();
        }
      };
      cond rc{std::move(stmt)};
      cb(rc);
    }

    void until(const bool& bvalue) {
      until([&] { return bvalue; });
    }
  };

  // -- constructors and destructors -------------------------------------------

  blocking_actor(actor_config& sys);

  ~blocking_actor();

  // -- overridden functions of abstract_actor ---------------------------------

  void enqueue(mailbox_element_ptr, execution_unit*) override;

  // -- overridden functions of local_actor ------------------------------------

  const char* name() const override;

  void launch(execution_unit* eu, bool lazy, bool hide) override;

  // -- virtual modifiers ------------------------------------------------------

  /// Implements the actor's behavior.
  virtual void act();

  // -- modifiers --------------------------------------------------------------

  /// Dequeues the next message from the mailbox that is
  /// matched by given behavior.
  template <class... Ts>
  void receive(Ts&&... xs) {
    accept_one_cond rc;
    varargs_receive(rc, message_id::make(), std::forward<Ts>(xs)...);
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
  receive_for_helper<T> receive_for(T& begin, T end) {
    return {this, begin, std::move(end)};
  }

  /// Receives messages as long as `stmt` returns true.
  /// Semantically equal to: `while (stmt()) { receive(...); }`.
  ///
  /// **Usage example:**
  /// ~~~
  /// int i = 0;
  /// receive_while([&]() { return (++i <= 10); }) (
  ///   ...
  /// );
  /// ~~~
  receive_while_helper receive_while(std::function<bool()> stmt);

  /// Receives messages as long as `ref` is true.
  /// Semantically equal to: `while (ref) { receive(...); }`.
  ///
  /// **Usage example:**
  /// ~~~
  /// bool running = true;
  /// receive_while(running) (
  ///   ...
  /// );
  /// ~~~
  receive_while_helper receive_while(const bool& ref);


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
    auto tup = std::make_tuple(std::forward<Ts>(xs)...);
    auto cb = [=](receive_cond& rc) mutable {
      varargs_tup_receive(rc, message_id::make(), tup);
    };
    return {cb};
  }

  /// Blocks this actor until all other actors are done.
  void await_all_other_actors_done();

  /// Blocks this actor until all `xs...` have terminated.
  template <class... Ts>
  void wait_for(Ts&&... xs) {
    using wait_for_atom = atom_constant<atom("waitFor")>;
    size_t expected = 0;
    size_t i = 0;
    size_t attach_results[] = {attach_functor(xs)...};
    for (auto res : attach_results)
      expected += res;
    receive_for(i, expected)(
      [](wait_for_atom) {
        // nop
      }
    );
  }

  /// Sets a user-defined exit reason `err`. This reason
  /// is signalized to other actors after `act()` returns.
  void fail_state(error err);

  // -- observers --------------------------------------------------------------

  /// Returns the current exit reason.
  inline const error& fail_state() {
    return fail_state_;
  }

  /// @cond PRIVATE

  /// Blocks until at least one message is in the mailbox.
  void await_data();

  /// Blocks until at least one message is in the mailbox or
  /// the absolute `timeout` was reached.
  bool await_data(timeout_type timeout);

  /// Receives messages until either a pre- or postcheck of `rcc` fails.
  template <class... Ts>
  void varargs_tup_receive(receive_cond& rcc, message_id mid,
                           std::tuple<Ts...>& tup) {
    using namespace detail;
    static_assert(sizeof...(Ts), "at least one argument required");
    // extract how many arguments are actually the behavior part,
    // i.e., neither `after(...) >> ...` nor `others >> ...`.
    using filtered =
      typename tl_filter_not<
        type_list<typename std::decay<Ts>::type...>,
        is_timeout_or_catch_all
      >::type;
    filtered tk;
    behavior bhvr{apply_args(make_behavior_impl, get_indices(tk), tup)};
    using tail_indices = typename il_range<
                           tl_size<filtered>::value, sizeof...(Ts)
                         >::type;
    make_blocking_behavior_t factory;
    auto fun = apply_args_prefixed(factory, tail_indices{}, tup, bhvr);
    receive_impl(rcc, mid, fun);
  }

  /// Receives messages until either a pre- or postcheck of `rcc` fails.
  template <class... Ts>
  void varargs_receive(receive_cond& rcc, message_id mid, Ts&&... xs) {
    auto tup = std::forward_as_tuple(std::forward<Ts>(xs)...);
    varargs_tup_receive(rcc, mid, tup);
  }

  /// Receives messages until either a pre- or postcheck of `rcc` fails.
  void receive_impl(receive_cond& rcc, message_id mid,
                    detail::blocking_behavior& bhvr);

  /// @endcond

private:
  size_t attach_functor(const actor&);

  size_t attach_functor(const actor_addr&);

  size_t attach_functor(const strong_actor_ptr&);

  template <class... Ts>
  size_t attach_functor(const typed_actor<Ts...>& x) {
    return attach_functor(actor_cast<strong_actor_ptr>(x));
  }

  template <class Container>
  size_t attach_functor(const Container& xs) {
    size_t res = 0;
    for (auto& x : xs)
      res += attach_functor(x);
    return res;
  }
};

} // namespace caf

#endif // CAF_BLOCKING_ACTOR_HPP
