// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_blocking_actor.hpp"
#include "caf/abstract_mailbox.hpp"
#include "caf/actor_config.hpp"
#include "caf/actor_traits.hpp"
#include "caf/after.hpp"
#include "caf/behavior.hpp"
#include "caf/blocking_response_handle.hpp"
#include "caf/callback.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/default_mailbox.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/dynamically_typed.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive/stack.hpp"
#include "caf/local_actor.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/mailer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/none.hpp"
#include "caf/policy/arg.hpp"
#include "caf/policy/blocking_requester.hpp"
#include "caf/telemetry/actor_metrics.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/typed_actor.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <type_traits>

namespace caf {

/// A thread-mapped or context-switching actor using a blocking
/// receive rather than a behavior-stack based message processing.
/// @extends local_actor
class CAF_CORE_EXPORT blocking_actor : public abstract_blocking_actor,
                                       public dynamically_typed_actor_base,
                                       public blocking_actor_base {
public:
  // -- nested and member types ------------------------------------------------

  /// Base type.
  using super = abstract_blocking_actor;

  /// Absolute timeout type.
  using timeout_type = std::chrono::high_resolution_clock::time_point;

  /// Supported behavior type.
  using behavior_type = behavior;

  /// Declared message passing interface.
  using signatures = none_t;

  // -- constants --------------------------------------------------------------

  static constexpr auto forced_spawn_options = spawn_options::detach_flag
                                               + spawn_options::blocking_flag;

  // -- nested classes ---------------------------------------------------------

  /// Represents pre- and postconditions for receive loops.
  class CAF_CORE_EXPORT receive_cond {
  public:
    virtual ~receive_cond();

    /// Returns whether a precondition for receiving a message still holds.
    virtual bool pre();

    /// Returns whether a postcondition for receiving a message still holds.
    virtual bool post();
  };

  /// Pseudo receive condition modeling a single receive.
  class CAF_CORE_EXPORT accept_one_cond : public receive_cond {
  public:
    ~accept_one_cond() override;
    bool post() override;
  };

  /// Implementation helper for `blocking_actor::receive_while`.
  struct CAF_CORE_EXPORT receive_while_helper {
    using fun_type = std::function<bool()>;

    blocking_actor* self;
    fun_type stmt_;

    template <class... Ts>
    void operator()(Ts&&... xs) {
      static_assert(sizeof...(Ts) > 0,
                    "operator() requires at least one argument");
      struct cond : receive_cond {
        fun_type stmt;
        explicit cond(fun_type x) : stmt(std::move(x)) {
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
        explicit cond(receive_for_helper& x) : outer(x) {
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
  struct CAF_CORE_EXPORT do_receive_helper {
    std::function<void(receive_cond& rc)> cb;

    template <class Statement>
    void until(Statement stmt) {
      struct cond : receive_cond {
        Statement f;
        explicit cond(Statement x) : f(std::move(x)) {
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

  // -- constructors and destructors -------------------------------------------

  explicit blocking_actor(actor_config& cfg);

  ~blocking_actor() override;

  // -- overridden functions of abstract_actor ---------------------------------

  bool enqueue(mailbox_element_ptr, scheduler*) override;

  // -- overridden functions of local_actor ------------------------------------

  const char* name() const override;

  bool initialize(scheduler* sched) override;

  consume_result consume(mailbox_element_ptr& what) override;

  bool launch_delayed() override;

  void launch(detail::private_thread* worker, scheduler* ctx) override;

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
    auto cb = [this, tup = std::make_tuple(std::forward<Ts>(xs)...)] //
      (receive_cond & rc) mutable {
        varargs_tup_receive(rc, make_message_id(), tup);
      };
    return {cb};
  }

  /// Blocks this actor until all other actors are done.
  void await_all_other_actors_done();

  /// Blocks this actor until all `xs...` have terminated.
  template <class... Ts>
  void wait_for(Ts&&... xs) {
    size_t expected = 0;
    size_t i = 0;
    const size_t attach_results[] = {attach_functor(xs)...};
    for (auto res : attach_results)
      expected += res;
    receive_for(i, expected)([](wait_for_atom) {
      // nop
    });
  }

  using super::fail_state;

  /// Sets a user-defined exit reason `err`. This reason
  /// is signalized to other actors after `act()` returns.
  void fail_state(error err);

  template <class... Args>
  auto mail(Args&&... args) {
    using mailer_policy = policy::blocking_requester;
    return make_mailer<mailer_policy>(this, std::forward<Args>(args)...);
  }

  CAF_ADD_DEPRECATED_REQUEST_API

  // -- monitoring -------------------------------------------------------------

  using super::monitor;

  using super::demonitor;

  /// Adds a unidirectional monitor to `whom` to receive a `down_msg` when
  /// `whom` terminates.
  /// @note Each call to `monitor` creates a new, independent monitor.
  template <message_priority P = message_priority::normal, class Handle>
  void monitor(const Handle& whom) {
    do_monitor(actor_cast<abstract_actor*>(whom), P);
  }

  /// Removes a monitor from `whom`.
  template <class Handle>
  void demonitor(const Handle& whom) {
    do_demonitor(actor_cast<strong_actor_ptr>(whom));
  }

  // -- customization points ---------------------------------------------------

  /// Blocks until at least one message is in the mailbox.
  virtual void await_data();

  /// Blocks until at least one message is in the mailbox or
  /// the absolute `timeout` was reached.
  virtual bool await_data(timeout_type timeout);

  /// Returns the next element from the mailbox or `nullptr`.
  mailbox_element_ptr dequeue();

  /// Returns the queue for storing incoming messages.
  abstract_mailbox& mailbox() {
    return *mailbox_;
  }
  /// @cond

  /// Receives messages until either a pre- or postcheck of `rcc` fails.
  template <class... Ts>
  void varargs_tup_receive(receive_cond& rcc, message_id mid,
                           std::tuple<Ts...>& tup) {
    using namespace detail;
    static_assert(sizeof...(Ts), "at least one argument is required");
    // extract how many arguments are actually the behavior part,
    // i.e., not `after(...) >> ...` (timeout definitions).
    static constexpr auto timeout_definitions
      = detail::tl_count_v<type_list<std::decay_t<Ts>...>,
                           is_timeout_definition>;
    if constexpr (timeout_definitions == 0) {
      behavior bhvr{std::apply(make_behavior_impl, std::move(tup))};
      detail::nop_callback<void()> on_timeout;
      receive_impl(rcc, mid, bhvr, infinite, on_timeout);
    } else {
      // If present, the timeout definition must be the last argument.
      static_assert(timeout_definitions == 1,
                    "only one timeout definition may be present");
      auto indices = std::make_index_sequence<sizeof...(Ts) - 1>{};
      behavior bhvr{apply_moved_args(make_behavior_impl, indices, tup)};
      static constexpr auto tail_index = sizeof...(Ts) - 1;
      auto& tail = std::get<tail_index>(tup);
      using timeout_type = std::remove_reference_t<decltype(tail)>;
      static_assert(is_timeout_definition_v<timeout_type>,
                    "the timeout definition must be the last argument");
      using handler_type = typename timeout_type::handler_type;
      using callback_type = callback_ref_impl<handler_type, void()>;
      callback_type on_timeout_cb{tail.handler};
      receive_impl(rcc, mid, bhvr, tail.timeout, on_timeout_cb);
    }
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

  void on_cleanup(const error& reason) override;

  // -- backwards compatibility ------------------------------------------------

  mailbox_element_ptr next_message() {
    return dequeue();
  }

  bool has_next_message() {
    return !mailbox_->empty();
  }

  /// @endcond

protected:
  /// Receives messages until either a pre- or postcheck of `rcc` fails.
  void receive_impl(receive_cond& rcc, message_id mid, behavior& bhvr,
                    timespan timeout, callback<void()>& on_timeout);

private:
  void do_unstash(mailbox_element_ptr ptr) override;

  void receive_impl(message_id mid, behavior& bhvr, timespan timeout) override;

  size_t attach_functor(const actor&);

  size_t attach_functor(const strong_actor_ptr&);

  void unstash();

  void close_mailbox();

  bool try_force_close_mailbox() final;

  template <class... Ts>
  size_t attach_functor(const typed_actor<Ts...>& x) {
    return attach_functor(actor_cast<strong_actor_ptr>(x));
  }

  template <class Container>
  size_t attach_functor(const Container& xs) {
    auto f = [this](size_t acc, const auto& x) {
      return acc + this->attach_functor(x);
    };
    return std::accumulate(xs.begin(), xs.end(), size_t{0}, f);
  }

  // -- member variables -------------------------------------------------------

  behavior* current_behavior_ = nullptr;

  message_id current_message_id_;

  /// Stores incoming messages.
  abstract_mailbox_ptr mailbox_;

  /// Stashes skipped messages until the actor processes the next message.
  intrusive::stack<mailbox_element> stash_;
};

} // namespace caf
