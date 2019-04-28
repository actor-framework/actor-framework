/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <chrono>
#include <mutex>
#include <condition_variable>

#include "caf/actor_config.hpp"
#include "caf/actor_marker.hpp"
#include "caf/after.hpp"
#include "caf/behavior.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/is_timeout_or_catch_all.hpp"
#include "caf/local_actor.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/none.hpp"
#include "caf/send.hpp"
#include "caf/typed_actor.hpp"

#include "caf/policy/arg.hpp"
#include "caf/policy/categorized.hpp"
#include "caf/policy/downstream_messages.hpp"
#include "caf/policy/normal_messages.hpp"
#include "caf/policy/upstream_messages.hpp"
#include "caf/policy/urgent_messages.hpp"

#include "caf/detail/apply_args.hpp"
#include "caf/detail/blocking_behavior.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"

#include "caf/intrusive/drr_cached_queue.hpp"
#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/fifo_inbox.hpp"
#include "caf/intrusive/wdrr_dynamic_multiplexed_queue.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"

#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
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
  // -- nested and member types ------------------------------------------------

  /// Base type.
  using super = extended_base;

  /// Stores asynchronous messages with default priority.
  using normal_queue = intrusive::drr_cached_queue<policy::normal_messages>;

  /// Stores asynchronous messages with high priority.
  using urgent_queue = intrusive::drr_cached_queue<policy::urgent_messages>;

  /// Configures the FIFO inbox with two nested queues:
  ///
  ///   1. Default asynchronous messages
  ///   2. High-priority asynchronous messages
  struct mailbox_policy {
    using deficit_type = size_t;

    using mapped_type = mailbox_element;

    using unique_pointer = mailbox_element_ptr;

    using queue_type = intrusive::wdrr_fixed_multiplexed_queue<
      policy::categorized, normal_queue, urgent_queue>;

    static constexpr size_t normal_queue_index = 0;

    static constexpr size_t urgent_queue_index = 1;
  };

  /// A queue optimized for single-reader-many-writers.
  using mailbox_type = intrusive::fifo_inbox<mailbox_policy>;

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
    ~accept_one_cond() override;
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
      self->varargs_receive(rc, make_message_id(), std::forward<Ts>(xs)...);
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
      self->varargs_receive(rc, make_message_id(), std::forward<Ts>(xs)...);
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
          return !f();
        }
      };
      cond rc{std::move(stmt)};
      cb(rc);
    }

    void until(const bool& bvalue) {
      until([&] { return bvalue; });
    }
  };

  struct mailbox_visitor {
    blocking_actor* self;
    bool& done;
    receive_cond& rcc;
    message_id mid;
    detail::blocking_behavior& bhvr;

    // Dispatches messages with high and normal priority to the same handler.
    template <class Queue>
    intrusive::task_result operator()(size_t, Queue&, mailbox_element& x) {
      return (*this)(x);
    }

    // Consumes `x`.
    intrusive::task_result operator()(mailbox_element& x);
  };

  // -- constructors and destructors -------------------------------------------

  blocking_actor(actor_config& cfg);

  ~blocking_actor() override;

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
    varargs_receive(rc, make_message_id(), std::forward<Ts>(xs)...);
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
      varargs_tup_receive(rc, make_message_id(), tup);
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

  // -- customization points ---------------------------------------------------

  /// Blocks until at least one message is in the mailbox.
  virtual void await_data();

  /// Blocks until at least one message is in the mailbox or
  /// the absolute `timeout` was reached.
  virtual bool await_data(timeout_type timeout);

  /// Returns the next element from the mailbox or `nullptr`.
  virtual mailbox_element_ptr dequeue();

  /// Returns the queue for storing incoming messages.
  inline mailbox_type& mailbox() {
    return mailbox_;
  }
  /// @cond PRIVATE

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
    behavior bhvr{apply_moved_args(make_behavior_impl, get_indices(tk), tup)};
    using tail_indices = typename il_range<
                           tl_size<filtered>::value, sizeof...(Ts)
                         >::type;
    make_blocking_behavior_t factory;
    auto fun = apply_moved_args_prefixed(factory, tail_indices{}, tup, &bhvr);
    receive_impl(rcc, mid, fun);
  }

  /// Receives messages until either a pre- or postcheck of `rcc` fails.
  void varargs_tup_receive(receive_cond& rcc, message_id mid,
                           std::tuple<behavior&>& tup);

  /// Receives messages until either a pre- or postcheck of `rcc` fails.
  template <class... Ts>
  void varargs_receive(receive_cond& rcc, message_id mid, Ts&&... xs) {
    auto tup = std::forward_as_tuple(std::forward<Ts>(xs)...);
    varargs_tup_receive(rcc, mid, tup);
  }

  /// Receives messages until either a pre- or postcheck of `rcc` fails.
  void receive_impl(receive_cond& rcc, message_id mid,
                    detail::blocking_behavior& bhvr);

  bool cleanup(error&& fail_state, execution_unit* host) override;

  sec build_pipeline(stream_slot in, stream_slot out, stream_manager_ptr mgr);

  // -- backwards compatibility ------------------------------------------------

  inline mailbox_element_ptr next_message() {
    return dequeue();
  }

  inline bool has_next_message() {
    return !mailbox_.empty();
  }

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

  // -- member variables -------------------------------------------------------

  // used by both event-based and blocking actors
  mailbox_type mailbox_;
};

} // namespace caf

