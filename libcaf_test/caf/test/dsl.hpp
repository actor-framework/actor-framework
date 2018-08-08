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

#include "caf/all.hpp"
#include "caf/config.hpp"
#include "caf/meta/annotation.hpp"
#include "caf/test/unit_test.hpp"

#include "caf/detail/gcd.hpp"

CAF_PUSH_WARNINGS

/// The type of `_`.
struct wildcard { };

/// Allows ignoring individual messages elements in `expect` clauses, e.g.
/// `expect((int, int), from(foo).to(bar).with(1, _))`.
constexpr wildcard _ = wildcard{};

/// @relates wildcard
constexpr bool operator==(const wildcard&, const wildcard&) {
  return true;
}

template <size_t I, class T>
bool cmp_one(const caf::message& x, const T& y) {
  if (std::is_same<T, wildcard>::value)
    return true;
  return x.match_element<T>(I) && x.get_as<T>(I) == y;
}

template <size_t I, class... Ts>
typename std::enable_if<(I == sizeof...(Ts)), bool>::type
msg_cmp_rec(const caf::message&, const std::tuple<Ts...>&) {
  return true;
}

template <size_t I, class... Ts>
typename std::enable_if<(I < sizeof...(Ts)), bool>::type
msg_cmp_rec(const caf::message& x, const std::tuple<Ts...>& ys) {
  return cmp_one<I>(x, std::get<I>(ys)) && msg_cmp_rec<I + 1>(x, ys);
}

// allow comparing arbitrary `T`s to `message` objects for the purpose of the
// testing DSL
namespace caf {

template <class... Ts>
bool operator==(const message& x, const std::tuple<Ts...>& y) {
  return x.size() == sizeof...(Ts) && msg_cmp_rec<0>(x, y);
}

template <class T>
bool operator==(const message& x, const T& y) {
  return x.match_elements<T>() && x.get_as<T>(0) == y;
}

} // namespace caf

// dummy function to force ADL later on
//int inspect(int, int);

template <class T>
struct has_outer_type {
  template <class U>
  static auto sfinae(U* x) -> typename U::outer_type*;

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using type = decltype(sfinae<T>(nullptr));
  static constexpr bool value = !std::is_same<type, std::false_type>::value;
};

// enables ADL in `with_content`
template <class T, class U>
T get(const has_outer_type<U>&);

// enables ADL in `with_content`
template <class T, class U>
bool is(const has_outer_type<U>&);

template <class Tup>
class elementwise_compare_inspector {
public:
  using result_type = bool;

  template <size_t X>
  using pos = std::integral_constant<size_t, X>;

  elementwise_compare_inspector(const Tup& xs) : xs_(xs) {
    // nop
  }

  template <class... Ts>
  bool operator()(const Ts&... xs) {
    return iterate(pos<0>{}, xs...);
  }

private:
  template <size_t X>
  bool iterate(pos<X>) {
    // end of recursion
    return true;
  }

  template <size_t X, class T, class... Ts>
  typename std::enable_if<
    caf::meta::is_annotation<T>::value,
    bool
  >::type
  iterate(pos<X> pos, const T&, const Ts&... ys) {
    return iterate(pos, ys...);
  }

  template <size_t X, class T, class... Ts>
  typename std::enable_if<
    !caf::meta::is_annotation<T>::value,
    bool
  >::type
  iterate(pos<X>, const T& y, const Ts&... ys) {
    std::integral_constant<size_t, X + 1> next;
    check(y, get<X>(xs_));
    return iterate(next, ys...);
  }

  template <class T, class U>
  static void check(const T& x, const U& y) {
    CAF_CHECK_EQUAL(x, y);
  }

  template <class T>
  static void check(const T&, const wildcard&) {
    // nop
  }

  const Tup& xs_;
};

// -- unified access to all actor handles in CAF -------------------------------

/// Reduces any of CAF's handle types to an `abstract_actor` pointer.
class caf_handle : caf::detail::comparable<caf_handle>,
                   caf::detail::comparable<caf_handle, std::nullptr_t> {
public:
  using pointer = caf::abstract_actor*;

  constexpr caf_handle() : ptr_(nullptr) {
    // nop
  }

  template <class T>
  caf_handle(const T& x) {
    *this = x;
  }

  caf_handle(const caf_handle&) = default;

  inline caf_handle& operator=(caf::abstract_actor* x)  {
    ptr_ = x;
    return *this;
  }

  template <class T,
            class E = caf::detail::enable_if_t<!std::is_pointer<T>::value>>
  caf_handle& operator=(const T& x) {
    ptr_ = caf::actor_cast<pointer>(x);
    return *this;
  }

  caf_handle& operator=(const caf_handle&) = default;

  pointer get() const {
    return ptr_;
  }

  ptrdiff_t compare(const caf_handle& other) const {
    return reinterpret_cast<ptrdiff_t>(ptr_)
           - reinterpret_cast<ptrdiff_t>(other.ptr_);
  }

  ptrdiff_t compare(std::nullptr_t) const {
    return reinterpret_cast<ptrdiff_t>(ptr_);
  }

private:
  caf::abstract_actor* ptr_;
};

// -- access to an actor's mailbox ---------------------------------------------

/// Returns a pointer to the next element in an actor's mailbox without taking
/// it out of the mailbox.
/// @pre `ptr` is alive and either a `scheduled_actor` or `blocking_actor`
inline caf::mailbox_element* next_mailbox_element(caf_handle x) {
  CAF_ASSERT(x.get() != nullptr);
  auto sptr = dynamic_cast<caf::scheduled_actor*>(x.get());
  if (sptr != nullptr) {
    return sptr->mailbox().closed() || sptr->mailbox().blocked()
           ? nullptr
           : sptr->mailbox().peek();
  }
  auto bptr = dynamic_cast<caf::blocking_actor*>(x.get());
  CAF_ASSERT(bptr != nullptr);
  return bptr->mailbox().closed() || bptr->mailbox().blocked()
         ? nullptr
         : bptr->mailbox().peek();
}

// -- introspection of the next mailbox element --------------------------------

/// @private
template <class... Ts>
caf::optional<std::tuple<Ts...>> default_extract(caf_handle x) {
  auto ptr = next_mailbox_element(x);
  if (ptr == nullptr || !ptr->content().template match_elements<Ts...>())
    return caf::none;
  return ptr->content().template get_as_tuple<Ts...>();
}

/// @private
template <class T>
caf::optional<std::tuple<T>> unboxing_extract(caf_handle x) {
  auto tup = default_extract<typename T::outer_type>(x);
  if (tup == caf::none || !is<T>(get<0>(*tup)))
    return caf::none;
  return std::make_tuple(get<T>(get<0>(*tup)));
}

/// Dispatches to `unboxing_extract` if
/// `sizeof...(Ts) == 0 && has_outer_type<T>::value`, otherwise dispatches to
/// `default_extract`.
/// @private
template <class T, bool HasOuterType, class... Ts>
struct try_extract_impl {
  caf::optional<std::tuple<T, Ts...>> operator()(caf_handle x) {
    return default_extract<T, Ts...>(x);
  }
};

template <class T>
struct try_extract_impl<T, true> {
  caf::optional<std::tuple<T>> operator()(caf_handle x) {
    return unboxing_extract<T>(x);
  }
};

/// Returns the content of the next mailbox element as `tuple<T, Ts...>` on a
/// match. Returns `none` otherwise.
template <class T, class... Ts>
caf::optional<std::tuple<T, Ts...>> try_extract(caf_handle x) {
  try_extract_impl<T, has_outer_type<T>::value, Ts...> f;
  return f(x);
}

/// Returns the content of the next mailbox element without taking it out of
/// the mailbox. Fails on an empty mailbox or if the content of the next
/// element does not match `<T, Ts...>`.
template <class T, class... Ts>
std::tuple<T, Ts...> extract(caf_handle x) {
  auto result = try_extract<T, Ts...>(x);
  if (result == caf::none) {
    auto ptr = next_mailbox_element(x);
    if (ptr == nullptr)
      CAF_FAIL("Mailbox is empty");
    CAF_FAIL("Message does not match expected pattern: "
             << to_string(ptr->content()));
  }
  return std::move(*result);
}

template <class T, class... Ts>
bool received(caf_handle x) {
  return try_extract<T, Ts...>(x) != caf::none;
}

template <class... Ts>
class expect_clause {
public:
  expect_clause(caf::scheduler::test_coordinator& sched)
      : sched_(sched),
        dest_(nullptr) {
    peek_ = [=] {
      /// The extractor will call CAF_FAIL on a type mismatch, essentially
      /// performing a type check when ignoring the result.
      extract<Ts...>(dest_);
    };
  }

  expect_clause(expect_clause&& other) = default;

  ~expect_clause() {
    if (peek_ != nullptr) {
      peek_();
      run_once();
    }
  }

  expect_clause& from(const wildcard&) {
    return *this;
  }

  template <class Handle>
  expect_clause& from(const Handle& whom) {
    src_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  template <class Handle>
  expect_clause& to(const Handle& whom) {
    CAF_REQUIRE(sched_.prioritize(whom));
    dest_ = &sched_.next_job<caf::scheduled_actor>();
    auto ptr = next_mailbox_element(dest_);
    CAF_REQUIRE(ptr != nullptr);
    if (src_)
      CAF_REQUIRE_EQUAL(ptr->sender, src_);
    return *this;
  }

  expect_clause& to(const caf::scoped_actor& whom) {
    dest_ = whom.ptr();
    return *this;
  }

  template <class... Us>
  void with(Us&&... xs) {
    // TODO: replace this workaround with the make_tuple() line when dropping
    //       support for GCC 4.8.
    std::tuple<typename std::decay<Us>::type...> tmp{std::forward<Us>(xs)...};
    //auto tmp = std::make_tuple(std::forward<Us>(xs)...);
    // TODO: move tmp into lambda when switching to C++14
    peek_ = [=] {
      using namespace caf::detail;
      elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
      auto ys = extract<Ts...>(dest_);
      auto ys_indices = get_indices(ys);
      CAF_REQUIRE(apply_args(inspector, ys_indices, ys));
    };
  }

protected:
  void run_once() {
    auto dptr = dynamic_cast<caf::blocking_actor*>(dest_);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  caf::scheduler::test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::local_actor* dest_;
  std::function<void ()> peek_;
};

template <class... Ts>
class allow_clause {
public:
  allow_clause(caf::scheduler::test_coordinator& sched)
      : sched_(sched),
        dest_(nullptr) {
    peek_ = [=] {
      if (dest_ != nullptr)
        return try_extract<Ts...>(dest_) != caf::none;
      return false;
    };
  }

  allow_clause(allow_clause&& other) = default;

  ~allow_clause() {
    eval();
  }

  allow_clause& from(const wildcard&) {
    return *this;
  }

  template <class Handle>
  allow_clause& from(const Handle& whom) {
    src_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  template <class Handle>
  allow_clause& to(const Handle& whom) {
    if (sched_.prioritize(whom))
      dest_ = &sched_.next_job<caf::scheduled_actor>();
    return *this;
  }

  allow_clause& to(const caf::scoped_actor& whom) {
    dest_ = whom.ptr();
    return *this;
  }

  template <class... Us>
  void with(Us&&... xs) {
    // TODO: move tmp into lambda when switching to C++14
    auto tmp = std::make_tuple(std::forward<Us>(xs)...);
    peek_ = [=] {
      using namespace caf::detail;
      elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
      auto ys = try_extract<Ts...>(dest_);
      if (ys != caf::none) {
        auto ys_indices = get_indices(*ys);
        return apply_args(inspector, ys_indices, *ys);
      }
      return false;
    };
  }

  bool eval() {
    if (peek_ != nullptr) {
      if (peek_()) {
        run_once();
        return true;
      }
    }
    return false;
  }

protected:
  void run_once() {
    auto dptr = dynamic_cast<caf::blocking_actor*>(dest_);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  caf::scheduler::test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::local_actor* dest_;
  std::function<bool ()> peek_;
};

template <class... Ts>
class disallow_clause {
public:
  disallow_clause() {
    check_ = [=] {
      auto ptr = next_mailbox_element(dest_);
      if (ptr == nullptr)
        return;
      if (src_ != nullptr && ptr->sender != src_)
        return;
      auto res = try_extract<Ts...>(dest_);
      if (res != caf::none)
        CAF_FAIL("received disallowed message: " << CAF_ARG(*res));
    };
  }

  disallow_clause(disallow_clause&& other) = default;

  ~disallow_clause() {
    if (check_ != nullptr)
      check_();
  }

  disallow_clause& from(const wildcard&) {
    return *this;
  }

  disallow_clause& from(caf_handle x) {
    src_ = x;
    return *this;
  }

  disallow_clause& to(caf_handle x) {
    dest_ = x;
    return *this;
  }

  template <class... Us>
  void with(Us&&... xs) {
    // TODO: move tmp into lambda when switching to C++14
    auto tmp = std::make_tuple(std::forward<Us>(xs)...);
    check_ = [=] {
      auto ptr = next_mailbox_element(dest_);
      if (ptr == nullptr)
        return;
      if (src_ != nullptr && ptr->sender != src_)
        return;
      auto res = try_extract<Ts...>(dest_);
      if (res != caf::none) {
        using namespace caf::detail;
        elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
        auto& ys = *res;
        auto ys_indices = get_indices(ys);
        if (apply_args(inspector, ys_indices, ys))
          CAF_FAIL("received disallowed message: " << CAF_ARG(*res));
      }
    };
  }

protected:
  caf_handle src_;
  caf_handle dest_;
  std::function<void ()> check_;
};

template <class... Ts>
struct test_coordinator_fixture_fetch_helper {
  template <class Self, class Interface>
  std::tuple<Ts...>
  operator()(caf::response_handle<Self, Interface, true>& from) const {
    std::tuple<Ts...> result;
    from.receive(
      [&](Ts&... xs) { result = std::make_tuple(std::move(xs)...); },
      [&](caf::error& err) { FAIL(from.self()->system().render(err)); });
    return result;
  }
};

template <class T>
struct test_coordinator_fixture_fetch_helper<T> {
  template <class Self, class Interface>
  T operator()(caf::response_handle<Self, Interface, true>& from) const {
    T result;
    from.receive(
      [&](T& x) { result = std::move(x); },
      [&](caf::error& err) { FAIL(from.self()->system().render(err)); });
    return result;
  }
};

/// A fixture with a deterministic scheduler setup.
template <class Config = caf::actor_system_config>
class test_coordinator_fixture {
public:
  // -- member types -----------------------------------------------------------

  /// A deterministic scheduler type.
  using scheduler_type = caf::scheduler::test_coordinator;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit test_coordinator_fixture(Ts&&... xs)
      : cfg(std::forward<Ts>(xs)...),
        sys(cfg.parse(caf::test::engine::argc(), caf::test::engine::argv())
               .set("scheduler.policy", caf::atom("testing"))
               .set("logger.inline-output", true)
               .set("middleman.network-backend", caf::atom("testing"))),
        self(sys, true),
        sched(dynamic_cast<scheduler_type&>(sys.scheduler())) {
    // Configure the clock to measure each batch item with 1us.
    sched.clock().time_per_unit.emplace(caf::atom("batch"),
                                        caf::timespan{1000});
    // Make sure the current time isn't 0.
    sched.clock().current_time += std::chrono::hours(1);
    credit_round_interval = cfg.stream_credit_round_interval;
  }

  virtual ~test_coordinator_fixture() {
    run();
  }

  // -- DSL functions ----------------------------------------------------------

  /// Allows the next actor to consume one message from its mailbox. Fails the
  /// test if no message was consumed.
  virtual bool consume_message() {
    return sched.try_run_once();
  }

  /// Allows each actors to consume all messages from its mailbox. Fails the
  /// test if no message was consumed.
  /// @returns The number of consumed messages.
  size_t consume_messages() {
    size_t result = 0;
    while (consume_message())
      ++result;
    return result;
  }

  /// Allows an simulated I/O devices to handle an event.
  virtual bool handle_io_event() {
    return false;
  }

  /// Allows each simulated I/O device to handle all events.
  size_t handle_io_events() {
    size_t result = 0;
    while (handle_io_event())
      ++result;
    return result;
  }

  /// Triggers the next pending timeout.
  virtual bool trigger_timeout() {
    return sched.trigger_timeout();
  }

  /// Triggers all pending timeouts.
  size_t trigger_timeouts() {
    size_t timeouts = 0;
    while (trigger_timeout())
      ++timeouts;
    return timeouts;
  }

  /// Advances the clock by `interval`.
  size_t advance_time(caf::timespan interval) {
    return sched.clock().advance_time(interval);
  }

  /// Consume messages and trigger timeouts until no activity remains.
  /// @returns The total number of events, i.e., messages consumed and
  ///          timeouts triggerd.
  size_t run() {
    return run_until([] { return false; });
  }

  /// Consume messages and trigger timeouts until `pred` becomes `true` or
  /// until no activity remains.
  /// @returns The total number of events, i.e., messages consumed and
  ///          timeouts triggered.
  template <class BoolPredicate>
  size_t run_until(BoolPredicate predicate) {
    CAF_LOG_TRACE("");
    // Bookkeeping.
    size_t events = 0;
    // Loop until no activity remains.
    for (;;) {
      size_t progress = 0;
      while (consume_message()) {
        ++progress;
        if (predicate()) {
          CAF_LOG_DEBUG("stop due to predicate:" << CAF_ARG(events));
          return events;
        }
      }
      while (handle_io_event()) {
        ++progress;
        if (predicate()) {
          CAF_LOG_DEBUG("stop due to predicate:" << CAF_ARG(events));
          return events;
        }
      }
      if (trigger_timeout())
        ++progress;
      if (progress == 0) {
        CAF_LOG_DEBUG("no activity left:" << CAF_ARG(events));
        return events;
      }
      events += progress;
    }
  }

  /// Call `run()` when the next scheduled actor becomes ready.
  void run_after_next_ready_event() {
    sched.after_next_enqueue([=] { run(); });
  }

  /// Call `run_until(predicate)` when the next scheduled actor becomes ready.
  template <class BoolPredicate>
  void run_until_after_next_ready_event(BoolPredicate predicate) {
    sched.after_next_enqueue([=] { run_until(predicate); });
  }

  /// Sends a request to `hdl`, then calls `run()`, and finally fetches and
  /// returns the result.
  template <class T, class... Ts, class Handle, class... Us>
  typename std::conditional<sizeof...(Ts) == 0, T, std::tuple<T, Ts...>>::type
  request(Handle hdl, Us... args) {
    auto res_hdl = self->request(hdl, caf::infinite, std::move(args)...);
    run();
    test_coordinator_fixture_fetch_helper<T, Ts...> f;
    return f(res_hdl);
  }

  /// Returns the next message from the next pending actor's mailbox as `T`.
  template <class T>
  const T& peek() {
    return sched.template peek<T>();
  }

  /// Dereferences `hdl` and downcasts it to `T`.
  template <class T = caf::scheduled_actor, class Handle = caf::actor>
  T& deref(const Handle& hdl) {
    auto ptr = caf::actor_cast<caf::abstract_actor*>(hdl);
    CAF_REQUIRE(ptr != nullptr);
    return dynamic_cast<T&>(*ptr);
  }

  // -- member variables -------------------------------------------------------

  /// The user-generated system config.
  Config cfg;

  /// Host system for (scheduled) actors.
  caf::actor_system sys;

  /// A scoped actor for conveniently sending and receiving messages.
  caf::scoped_actor self;

  /// Deterministic scheduler.
  scheduler_type& sched;

  // -- deprecated functionality -----------------------------------------------

  void run_exhaustively() CAF_DEPRECATED_MSG("use run() instead");

  void run_exhaustively_until(std::function<bool()> f)
    CAF_DEPRECATED_MSG("use run_until() instead");

  void loop_after_next_enqueue()
    CAF_DEPRECATED_MSG("use run_after_next_ready_event() instead");

  caf::timespan credit_round_interval CAF_DEPRECATED;
};

template <class Config>
void test_coordinator_fixture<Config>::run_exhaustively() {
  run();
}

template <class Config>
void test_coordinator_fixture<Config>::run_exhaustively_until(
  std::function<bool()> f) {
  run_until(std::move(f));
}

template <class Config>
void test_coordinator_fixture<Config>::loop_after_next_enqueue() {
  sched.after_next_enqueue([=] { run(); });
}

/// Unboxes an expected value or fails the test if it doesn't exist.
template <class T>
T unbox(caf::expected<T> x) {
  if (!x)
    CAF_FAIL(to_string(x.error()));
  return std::move(*x);
}

/// Unboxes an optional value or fails the test if it doesn't exist.
template <class T>
T unbox(caf::optional<T> x) {
  if (!x)
    CAF_FAIL("x == none");
  return std::move(*x);
}

/// Expands to its argument.
#define CAF_EXPAND(x) x

/// Expands to its arguments.
#define CAF_DSL_LIST(...) __VA_ARGS__

/// Convenience macro for defining expect clauses.
#define expect(types, fields)                                                  \
  do {                                                                         \
    CAF_MESSAGE("expect" << #types << "." << #fields);                         \
    expect_clause<CAF_EXPAND(CAF_DSL_LIST types)>{sched}.fields;               \
  } while (false)

/// Convenience macro for defining allow clauses.
#define allow(types, fields)                                                   \
  ([&] {                                                                       \
    CAF_MESSAGE("allow" << #types << "." << #fields);                          \
    allow_clause<CAF_EXPAND(CAF_DSL_LIST types)> x{sched};                     \
    x.fields;                                                                  \
    return x.eval();                                                           \
  })()

/// Convenience macro for defining disallow clauses.
#define disallow(types, fields)                                                \
  do {                                                                         \
    CAF_MESSAGE("disallow" << #types << "." << #fields);                       \
    disallow_clause<CAF_EXPAND(CAF_DSL_LIST types)>{}.fields;                  \
  } while (false)

/// Defines the required base type for testee states in the current namespace.
#define TESTEE_SETUP()                                                         \
  template <class T>                                                           \
  struct testee_state_base {}

/// Convenience macro for adding additional state to a testee.
#define TESTEE_STATE(tname)                                                    \
  struct tname##_state;                                                        \
  template <>                                                                  \
  struct testee_state_base<tname##_state>

/// Implementation detail for `TESTEE` and `VARARGS_TESTEE`.
#define TESTEE_SCAFFOLD(tname)                                                 \
  struct tname##_state : testee_state_base<tname##_state> {                    \
    static const char* name;                                                   \
  };                                                                           \
  const char* tname##_state::name = #tname;                                    \
  using tname##_actor = stateful_actor<tname##_state>

/// Convenience macro for defining an actor named `tname`.
#define TESTEE(tname)                                                          \
  TESTEE_SCAFFOLD(tname);                                                      \
  behavior tname(tname##_actor* self)

/// Convenience macro for defining an actor named `tname` with any number of
/// initialization arguments.
#define VARARGS_TESTEE(tname, ...)                                             \
  TESTEE_SCAFFOLD(tname);                                                      \
  behavior tname(tname##_actor* self, __VA_ARGS__)

CAF_POP_WARNINGS
