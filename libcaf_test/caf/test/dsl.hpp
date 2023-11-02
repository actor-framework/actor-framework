// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/config.hpp"
#include "caf/init_global_meta_objects.hpp"

#include <tuple>
#include <type_traits>

CAF_PUSH_WARNINGS

/// The type of `_`.
struct wildcard {};

/// Allows ignoring individual messages elements in `expect` clauses, e.g.
/// `expect((int, int), from(foo).to(bar).with(1, _))`.
constexpr wildcard _ = wildcard{}; // NOLINT(bugprone-reserved-identifier)

/// @relates wildcard
constexpr bool operator==(const wildcard&, const wildcard&) {
  return true;
}

template <size_t I, class T>
bool cmp_one(const caf::message& x, const T& y) {
  if (std::is_same_v<T, wildcard>)
    return true;
  return x.match_element<T>(I) && x.get_as<T>(I) == y;
}

template <size_t I, class... Ts>
std::enable_if_t<(I == sizeof...(Ts)), bool>
msg_cmp_rec(const caf::message&, const std::tuple<Ts...>&) {
  return true;
}

template <size_t I, class... Ts>
std::enable_if_t<(I < sizeof...(Ts)), bool>
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
// int inspect(int, int);

template <class T>
struct has_outer_type {
  template <class U>
  static auto sfinae(U* x) -> typename U::outer_type*;

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using type = decltype(sfinae<T>(nullptr));
  static constexpr bool value = !std::is_same_v<type, std::false_type>;
};

// enables ADL in `with_content`
template <class T, class U>
T get(const has_outer_type<U>&);

// enables ADL in `with_content`
template <class T, class U>
bool is(const has_outer_type<U>&);

template <class Tuple>
class elementwise_compare_inspector {
public:
  using result_type = bool;

  static constexpr size_t num_values = std::tuple_size_v<Tuple>;

  explicit elementwise_compare_inspector(const Tuple& xs) : xs_(xs) {
    // nop
  }

  template <class... Ts>
  bool operator()(const Ts&... xs) const {
    static_assert(sizeof...(Ts) == num_values);
    return apply(std::forward_as_tuple(xs...),
                 std::index_sequence_for<Ts...>{});
  }

private:
  template <class OtherTuple, size_t... Is>
  bool apply(OtherTuple values, std::index_sequence<Is...>) const {
    using std::get;
    return (check(get<Is>(values), get<Is>(xs_)) && ...);
  }

  template <class T, class U>
  static bool check(const T& x, const U& y) {
    return x == y;
  }

  template <class T>
  static bool check(const T&, const wildcard&) {
    return true;
  }

  const Tuple& xs_;
};

// -- unified access to all actor handles in CAF -------------------------------

/// Reduces any of CAF's handle types to an `abstract_actor` pointer.
class caf_handle : caf::detail::comparable<caf_handle>,
                   caf::detail::comparable<caf_handle, std::nullptr_t> {
public:
  using pointer = caf::abstract_actor*;

  constexpr caf_handle(pointer ptr = nullptr) : ptr_(ptr) {
    // nop
  }

  caf_handle(const caf::strong_actor_ptr& x) {
    set(x);
  }

  caf_handle(const caf::actor& x) {
    set(x);
  }

  caf_handle(const caf::actor_addr& x) {
    set(x);
  }

  caf_handle(const caf::scoped_actor& x) {
    set(x);
  }

  template <class... Ts>
  caf_handle(const caf::typed_actor<Ts...>& x) {
    set(x);
  }

  caf_handle(const caf_handle&) = default;

  caf_handle& operator=(pointer x) {
    ptr_ = x;
    return *this;
  }

  template <class T, class E = std::enable_if_t<!std::is_pointer_v<T>>>
  caf_handle& operator=(const T& x) {
    set(x);
    return *this;
  }

  caf_handle& operator=(const caf_handle&) = default;

  pointer get() const {
    return ptr_;
  }

  pointer operator->() const {
    return get();
  }

  ptrdiff_t compare(const caf_handle& other) const {
    return reinterpret_cast<ptrdiff_t>(ptr_)
           - reinterpret_cast<ptrdiff_t>(other.ptr_);
  }

  ptrdiff_t compare(std::nullptr_t) const {
    return reinterpret_cast<ptrdiff_t>(ptr_);
  }

private:
  template <class T>
  void set(const T& x) {
    ptr_ = caf::actor_cast<pointer>(x);
  }

  caf::abstract_actor* ptr_;
};

// -- introspection of the next mailbox element --------------------------------

/// @private
template <class... Ts>
std::optional<std::tuple<Ts...>> default_extract(caf_handle x) {
  auto ptr = x->peek_at_next_mailbox_element();
  if (ptr == nullptr)
    return std::nullopt;
  if (auto view = caf::make_const_typed_message_view<Ts...>(ptr->content()))
    return to_tuple(view);
  return std::nullopt;
}

/// @private
template <class T>
std::optional<std::tuple<T>> unboxing_extract(caf_handle x) {
  auto tup = default_extract<typename T::outer_type>(x);
  if (!tup || !is<T>(get<0>(*tup)))
    return std::nullopt;
  return std::make_tuple(get<T>(get<0>(*tup)));
}

/// Dispatches to `unboxing_extract` if
/// `sizeof...(Ts) == 0 && has_outer_type<T>::value`, otherwise dispatches to
/// `default_extract`.
/// @private
template <class T, bool HasOuterType, class... Ts>
struct try_extract_impl {
  std::optional<std::tuple<T, Ts...>> operator()(caf_handle x) {
    return default_extract<T, Ts...>(x);
  }
};

template <class T>
struct try_extract_impl<T, true> {
  std::optional<std::tuple<T>> operator()(caf_handle x) {
    return unboxing_extract<T>(x);
  }
};

/// Returns the content of the next mailbox element as `tuple<T, Ts...>` on a
/// match. Returns `std::nullopt` otherwise.
template <class T, class... Ts>
std::optional<std::tuple<T, Ts...>> try_extract(caf_handle x) {
  try_extract_impl<T, has_outer_type<T>::value, Ts...> f;
  return f(x);
}

/// Returns the content of the next mailbox element without taking it out of
/// the mailbox. Fails on an empty mailbox or if the content of the next
/// element does not match `<T, Ts...>`.
template <class... Ts>
std::tuple<Ts...> extract(caf_handle x, int src_line) {
  if constexpr (sizeof...(Ts) > 0) {
    if (auto result = try_extract<Ts...>(x)) {
      return std::move(*result);
    } else {
      auto ptr = x->peek_at_next_mailbox_element();
      if (ptr == nullptr)
        CAF_FAIL("cannot peek at the next message: mailbox is empty", src_line);
      else
        CAF_FAIL("message does not match expected types: "
                   << to_string(ptr->content()),
                 src_line);
    }
  } else {
    auto ptr = x->peek_at_next_mailbox_element();
    if (ptr == nullptr)
      CAF_FAIL("cannot peek at the next message: mailbox is empty", src_line);
    else if (!ptr->content().empty())
      CAF_FAIL("message does not match (expected an empty message): "
                 << to_string(ptr->content()),
               src_line);
    return {};
  }
}

template <class T, class... Ts>
bool received(caf_handle x) {
  return try_extract<T, Ts...>(x) != std::nullopt;
}

class test_coordinator : public caf::scheduler::abstract_coordinator {
public:
  using super = caf::scheduler::abstract_coordinator;

  using super::super;

  /// A double-ended queue representing our current job queue.
  std::deque<caf::resumable*> jobs;

  template <class T = caf::resumable>
  T& next_job() {
    if (jobs.empty())
      CAF_RAISE_ERROR("jobs.empty()");
    return dynamic_cast<T&>(*jobs.front());
  }

  template <class Handle>
  bool prioritize(const Handle& x) {
    auto ptr
      = dynamic_cast<caf::resumable*>(caf::actor_cast<caf::abstract_actor*>(x));
    return prioritize_impl(ptr);
  }

  virtual void run_once() = 0;

private:
  virtual bool prioritize_impl(caf::resumable* ptr) = 0;
};

template <class... Ts>
class expect_clause {
public:
  explicit expect_clause(test_coordinator& sched, int src_line)
    : sched_(sched), src_line_(src_line) {
    peek_ = [this] {
      /// The extractor will call CAF_FAIL on a type mismatch, essentially
      /// performing a type check when ignoring the result.
      extract<Ts...>(dest_, src_line_);
    };
  }

  expect_clause(expect_clause&& other) = default;

  void eval(const char* type_str, const char* fields_str) {
    using namespace caf;
    test::logger::instance().verbose()
      << term::yellow << "  -> " << term::reset
      << test::logger::stream::reset_flags_t{} << "expect " << type_str << "."
      << fields_str << " [line " << src_line_ << "]\n";
    peek_();
    run_once();
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
    if (!sched_.prioritize(whom))
      CAF_FAIL("there is no message for the designated receiver", src_line_);
    dest_ = &sched_.next_job<caf::abstract_actor>();
    auto ptr = dest_->peek_at_next_mailbox_element();
    if (ptr == nullptr)
      CAF_FAIL("the designated receiver has no message in its mailbox",
               src_line_);
    if (src_ && ptr->sender != src_)
      CAF_FAIL("the found message is not from the expected sender", src_line_);
    return *this;
  }

  expect_clause& to(const caf::scoped_actor& whom) {
    dest_ = caf::actor_cast<caf::abstract_actor*>(whom);
    return *this;
  }

  template <class... Us>
  expect_clause& with(Us&&... xs) {
    peek_ = [this, tmp = std::make_tuple(std::forward<Us>(xs)...)] {
      auto inspector = elementwise_compare_inspector<decltype(tmp)>{tmp};
      auto content = extract<Ts...>(dest_, src_line_);
      if (!std::apply(inspector, content))
        CAF_FAIL("message does not match expected content: "
                   << caf::deep_to_string(tmp) << " vs "
                   << caf::deep_to_string(content),
                 src_line_);
    };
    return *this;
  }

protected:
  void run_once() {
    auto dptr = dynamic_cast<caf::blocking_actor*>(dest_);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::abstract_actor* dest_ = nullptr;
  std::function<void()> peek_;
  int src_line_;
};

template <>
class expect_clause<void> {
public:
  explicit expect_clause(test_coordinator& sched, int src_line)
    : sched_(sched), src_line_(src_line) {
    // nop
  }

  expect_clause(expect_clause&& other) = default;

  void eval(const char*, const char* fields_str) {
    using namespace caf;
    test::logger::instance().verbose()
      << term::yellow << "  -> " << term::reset
      << test::logger::stream::reset_flags_t{} << "expect(void)." << fields_str
      << " [line " << src_line_ << "]\n";
    auto ptr = dest_->peek_at_next_mailbox_element();
    if (ptr == nullptr)
      CAF_FAIL("no message found", src_line_);
    if (!ptr->content().empty())
      CAF_FAIL("non-empty message found: " << to_string(ptr->content()),
               src_line_);
    run_once();
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
    dest_ = &sched_.next_job<caf::abstract_actor>();
    auto ptr = dest_->peek_at_next_mailbox_element();
    CAF_REQUIRE(ptr != nullptr);
    if (src_)
      CAF_REQUIRE_EQUAL(ptr->sender, src_);
    return *this;
  }

  expect_clause& to(const caf::scoped_actor& whom) {
    dest_ = caf::actor_cast<caf::abstract_actor*>(whom);
    return *this;
  }

protected:
  void run_once() {
    auto dptr = dynamic_cast<caf::blocking_actor*>(dest_);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::abstract_actor* dest_ = nullptr;
  int src_line_;
};

template <class... Ts>
class inject_clause {
public:
  explicit inject_clause(test_coordinator& sched, int src_line)
    : sched_(sched), src_line_(src_line) {
    // nop
  }

  inject_clause(inject_clause&& other) = default;

  template <class Handle>
  inject_clause& from(const Handle& whom) {
    src_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  template <class Handle>
  inject_clause& to(const Handle& whom) {
    dest_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  inject_clause& with(Ts... xs) {
    msg_ = caf::make_message(std::move(xs)...);
    return *this;
  }

  void eval(const char* type_str, const char* fields_str) {
    using namespace caf;
    test::logger::instance().verbose()
      << term::yellow << "  -> " << term::reset
      << test::logger::stream::reset_flags_t{} << "inject" << type_str << "."
      << fields_str << " [line " << src_line_ << "]\n";
    if (dest_ == nullptr)
      CAF_FAIL("missing .to() in inject() statement", src_line_);
    else if (src_ == nullptr)
      caf::anon_send(caf::actor_cast<caf::actor>(dest_), msg_);
    else
      caf::send_as(caf::actor_cast<caf::actor>(src_),
                   caf::actor_cast<caf::actor>(dest_), msg_);
    if (!sched_.prioritize(dest_))
      CAF_FAIL("inject: failed to schedule destination actor", src_line_);
    auto dest_ptr = &sched_.next_job<caf::abstract_actor>();
    auto ptr = dest_ptr->peek_at_next_mailbox_element();
    if (ptr == nullptr)
      CAF_FAIL("inject: failed to get next message from destination actor",
               src_line_);
    if (ptr->sender != src_)
      CAF_FAIL("inject: found unexpected sender for the next message",
               src_line_);
    if (ptr->payload.cptr() != msg_.cptr())
      CAF_FAIL("inject: found unexpected message => " << ptr->payload << " !! "
                                                      << msg_,
               src_line_);
    msg_.reset(); // drop local reference before running the actor
    run_once();
  }

protected:
  void run_once() {
    auto ptr = caf::actor_cast<caf::abstract_actor*>(dest_);
    auto dptr = dynamic_cast<caf::blocking_actor*>(ptr);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::strong_actor_ptr dest_;
  caf::message msg_;
  int src_line_;
};

template <class... Ts>
class allow_clause {
public:
  explicit allow_clause(test_coordinator& sched, int src_line)
    : sched_(sched), src_line_(src_line) {
    peek_ = [this] {
      if (dest_ != nullptr)
        return try_extract<Ts...>(dest_) != std::nullopt;
      else
        return false;
    };
  }

  allow_clause(allow_clause&& other) = default;

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
      dest_ = &sched_.next_job<caf::abstract_actor>();
    else if (auto ptr = caf::actor_cast<caf::abstract_actor*>(whom))
      dest_ = dynamic_cast<caf::blocking_actor*>(ptr);
    return *this;
  }

  template <class... Us>
  allow_clause& with(Us&&... xs) {
    peek_ = [this, tmp = std::make_tuple(std::forward<Us>(xs)...)] {
      using namespace caf::detail;
      if (dest_ != nullptr) {
        if (auto ys = try_extract<Ts...>(dest_)) {
          elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
          auto ys_indices = get_indices(*ys);
          return apply_args(inspector, ys_indices, *ys);
        }
      }
      return false;
    };
    return *this;
  }

  bool eval(const char* type_str, const char* fields_str) {
    using namespace caf;
    test::logger::instance().verbose()
      << term::yellow << "  -> " << term::reset
      << test::logger::stream::reset_flags_t{} << "allow" << type_str << "."
      << fields_str << " [line " << src_line_ << "]\n";
    if (!dest_)
      return false;
    if (auto msg_ptr = dest_->peek_at_next_mailbox_element();
        !msg_ptr || (src_ && msg_ptr->sender != src_))
      return false;
    if (peek_()) {
      run_once();
      return true;
    } else {
      return false;
    }
  }

protected:
  void run_once() {
    auto dptr = dynamic_cast<caf::blocking_actor*>(dest_);
    if (dptr == nullptr)
      sched_.run_once();
    else
      dptr->dequeue(); // Drop message.
  }

  test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::abstract_actor* dest_ = nullptr;
  std::function<bool()> peek_;
  int src_line_;
};

template <class... Ts>
class disallow_clause {
public:
  disallow_clause(int src_line) : src_line_(src_line) {
    check_ = [this] {
      auto ptr = dest_->peek_at_next_mailbox_element();
      if (ptr == nullptr)
        return;
      if (src_ != nullptr && ptr->sender != src_)
        return;
      auto res = try_extract<Ts...>(dest_);
      if (res)
        CAF_FAIL("received disallowed message: " << caf::deep_to_string(*ptr),
                 src_line_);
    };
  }

  disallow_clause(disallow_clause&& other) = default;

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
  disallow_clause& with(Us&&... xs) {
    check_ = [this, tmp = std::make_tuple(std::forward<Us>(xs)...)] {
      auto ptr = dest_->peek_at_next_mailbox_element();
      if (ptr == nullptr)
        return;
      if (src_ != nullptr && ptr->sender != src_)
        return;
      auto res = try_extract<Ts...>(dest_);
      if (res != std::nullopt) {
        using namespace caf::detail;
        elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
        auto& ys = *res;
        auto ys_indices = get_indices(ys);
        if (apply_args(inspector, ys_indices, ys))
          CAF_FAIL("received disallowed message: " << CAF_ARG(*res), src_line_);
      }
    };
    return *this;
  }

  void eval(const char* type_str, const char* fields_str) {
    using namespace caf;
    test::logger::instance().verbose()
      << term::yellow << "  -> " << term::reset
      << test::logger::stream::reset_flags_t{} << "disallow" << type_str << "."
      << fields_str << " [line " << src_line_ << "]\n";
    check_();
  }

protected:
  caf_handle src_;
  caf_handle dest_;
  std::function<void()> check_;
  int src_line_;
};

template <class... Ts>
struct test_coordinator_fixture_fetch_helper {
  template <class Self, template <class> class Policy, class Interface>
  std::tuple<Ts...>
  operator()(caf::response_handle<Self, Policy<Interface>>& from) const {
    std::tuple<Ts...> result;
    from.receive([&](Ts&... xs) { result = std::make_tuple(std::move(xs)...); },
                 [&](caf::error& err) { CAF_FAIL(err); });
    return result;
  }
};

template <class T>
struct test_coordinator_fixture_fetch_helper<T> {
  template <class Self, template <class> class Policy, class Interface>
  T operator()(caf::response_handle<Self, Policy<Interface>>& from) const {
    T result;
    from.receive([&](T& x) { result = std::move(x); },
                 [&](caf::error& err) { CAF_FAIL(err); });
    return result;
  }
};

/// A fixture with a deterministic scheduler setup.
template <class Config = caf::actor_system_config>
class test_coordinator_fixture {
public:
  // -- member types -----------------------------------------------------------

  class test_actor_clock : public caf::actor_clock {
  public:
    // -- constructors, destructors, and assignment operators ------------------

    test_actor_clock() : current_time(duration_type{1}) {
      // This ctor makes sure that the clock isn't at the default-constructed
      // time_point, because begin-of-epoch may have special meaning.
    }

    // -- overrides ------------------------------------------------------------

    time_point now() const noexcept override {
      return current_time;
    }

    caf::disposable schedule(time_point abs_time, caf::action f) override {
      CAF_ASSERT(f.ptr() != nullptr);
      actions.emplace(abs_time, f);
      return std::move(f).as_disposable();
    }

    // -- testing DSL API ------------------------------------------------------

    /// Returns whether the actor clock has at least one pending timeout.
    bool has_pending_timeout() const {
      auto not_disposed = [](const auto& kvp) {
        return !kvp.second.disposed();
      };
      return std::any_of(actions.begin(), actions.end(), not_disposed);
    }

    /// Triggers the next pending timeout regardless of its timestamp. Sets
    /// `current_time` to the time point of the triggered timeout unless
    /// `current_time` is already set to a later time.
    /// @returns Whether a timeout was triggered.
    bool trigger_timeout() {
      for (;;) {
        if (actions.empty())
          return false;
        auto i = actions.begin();
        auto t = i->first;
        if (t > current_time)
          current_time = t;
        if (try_trigger_once())
          return true;
      }
    }

    /// Triggers all pending timeouts regardless of their timestamp. Sets
    /// `current_time` to the time point of the latest timeout unless
    /// `current_time` is already set to a later time.
    /// @returns The number of triggered timeouts.
    size_t trigger_timeouts() {
      if (actions.empty())
        return 0u;
      size_t result = 0;
      while (trigger_timeout())
        ++result;
      return result;
    }

    /// Advances the time by `x` and dispatches timeouts and delayed messages.
    /// @returns The number of triggered timeouts.
    size_t advance_time(duration_type x) {
      current_time += x;
      auto result = size_t{0};
      while (!actions.empty() && actions.begin()->first <= current_time)
        if (try_trigger_once())
          ++result;
      return result;
    }

    // -- properties -----------------------------------------------------------

    /// @pre has_pending_timeout()
    time_point next_timeout() const {
      return actions.begin()->first;
    }

    // -- member variables -----------------------------------------------------

    time_point current_time;

    std::multimap<time_point, caf::action> actions;

  private:
    bool try_trigger_once() {
      for (;;) {
        if (actions.empty())
          return false;
        auto i = actions.begin();
        auto [t, f] = *i;
        if (t > current_time)
          return false;
        actions.erase(i);
        if (!f.disposed()) {
          f.run();
          return true;
        }
      }
    }
  };

  /// A deterministic scheduler type.
  class test_coordinator_impl : public test_coordinator {
  public:
    using super = abstract_coordinator;

    class dummy_worker : public caf::execution_unit {
    public:
      dummy_worker(test_coordinator* parent)
        : execution_unit(&parent->system()), parent_(parent) {
        // nop
      }

      void exec_later(caf::resumable* ptr) override {
        parent_->jobs.push_back(ptr);
      }

    private:
      test_coordinator* parent_;
    };

    class dummy_printer : public caf::monitorable_actor {
    public:
      dummy_printer(caf::actor_config& cfg) : monitorable_actor(cfg) {
        mh_.assign([&](caf::add_atom, caf::actor_id, const std::string& str) {
          printf("%s", str.c_str());
        });
      }

      bool enqueue(caf::mailbox_element_ptr what,
                   caf::execution_unit*) override {
        mh_(what->content());
        return true;
      }

      void setup_metrics() {
        // nop
      }

    private:
      caf::message_handler mh_;
    };

    /// A type-erased boolean predicate.
    using bool_predicate = std::function<bool()>;

    test_coordinator_impl(caf::actor_system& sys) : test_coordinator(sys) {
      // nop
    }

    /// Returns whether at least one job is in the queue.
    bool has_job() const {
      return !jobs.empty();
    }

    /// Peeks into the mailbox of `next_job<scheduled_actor>()`.
    template <class... Ts>
    decltype(auto) peek() {
      auto ptr
        = next_job<caf::scheduled_actor>().peek_at_next_mailbox_element();
      CAF_ASSERT(ptr != nullptr);
      if (auto view = caf::make_const_typed_message_view<Ts...>(ptr->payload)) {
        if constexpr (sizeof...(Ts) == 1)
          return get<0>(view);
        else
          return to_tuple(view);
      } else {
        CAF_RAISE_ERROR("Mailbox element does not match.");
      }
    }

    /// Puts `x` at the front of the queue unless it cannot be found in the
    /// queue. Returns `true` if `x` exists in the queue and was put in front,
    /// `false` otherwise.
    bool prioritize_impl(caf::resumable* ptr) override {
      if (!ptr)
        return false;
      auto i = std::find(jobs.begin(), jobs.end(), ptr);
      if (i == jobs.end())
        return false;
      if (i == jobs.begin())
        return true;
      std::rotate(jobs.begin(), i, i + 1);
      return true;
    }

    /// Runs all jobs that satisfy the predicate.
    template <class Predicate>
    size_t run_jobs_filtered(Predicate predicate) {
      size_t result = 0;
      while (!jobs.empty()) {
        auto i = std::find_if(jobs.begin(), jobs.end(), predicate);
        if (i == jobs.end())
          return result;
        if (i != jobs.begin())
          std::rotate(jobs.begin(), i, i + 1);
        run_once();
        ++result;
      }
      return result;
    }

    /// Tries to execute a single event in FIFO order.
    bool try_run_once() {
      if (jobs.empty())
        return false;
      auto job = jobs.front();
      jobs.pop_front();
      dummy_worker worker{this};
      switch (job->resume(&worker, 1)) {
        case caf::resumable::resume_later:
          jobs.push_front(job);
          break;
        case caf::resumable::done:
        case caf::resumable::awaiting_message:
          intrusive_ptr_release(job);
          break;
        case caf::resumable::shutdown_execution_unit:
          break;
      }
      return true;
    }

    /// Tries to execute a single event in LIFO order.
    bool try_run_once_lifo() {
      if (jobs.empty())
        return false;
      if (jobs.size() >= 2)
        std::rotate(jobs.rbegin(), jobs.rbegin() + 1, jobs.rend());
      return try_run_once();
    }

    /// Executes a single event in FIFO order or fails if no event is available.
    void run_once() override {
      if (jobs.empty())
        CAF_RAISE_ERROR("No job to run available.");
      try_run_once();
    }

    /// Executes a single event in LIFO order or fails if no event is available.
    void run_once_lifo() {
      if (jobs.empty())
        CAF_RAISE_ERROR("No job to run available.");
      try_run_once_lifo();
    }

    /// Executes events until the job queue is empty and no pending timeouts are
    /// left. Returns the number of processed events.
    size_t run(size_t max_count = std::numeric_limits<size_t>::max()) {
      size_t res = 0;
      while (res < max_count && try_run_once())
        ++res;
      return res;
    }

    /// Returns whether at least one pending timeout exists.
    bool has_pending_timeout() const {
      return clock_.has_pending_timeout();
    }

    /// Tries to trigger a single timeout.
    bool trigger_timeout() {
      return clock_.trigger_timeout();
    }

    /// Triggers all pending timeouts.
    size_t trigger_timeouts() {
      return clock_.trigger_timeouts();
    }

    /// Advances simulation time and returns the number of triggered timeouts.
    size_t advance_time(caf::timespan x) {
      return clock_.advance_time(x);
    }

    /// Call `f` after the next enqueue operation.
    template <class F>
    void after_next_enqueue(F f) {
      after_next_enqueue_ = f;
    }

    /// Executes the next enqueued job immediately by using the
    /// `after_next_enqueue` hook.
    void inline_next_enqueue() {
      after_next_enqueue([this] { run_once_lifo(); });
    }

    /// Executes all enqueued jobs immediately by using the `after_next_enqueue`
    /// hook.
    void inline_all_enqueues() {
      after_next_enqueue([this] { inline_all_enqueues_helper(); });
    }

    bool detaches_utility_actors() const override {
      return false;
    }

    test_actor_clock& clock() noexcept override {
      return clock_;
    }

  protected:
    void start() override {
      dummy_worker worker{this};
      caf::actor_config cfg{&worker};
      auto& sys = system();
      utility_actors_[printer_id] = caf::make_actor<dummy_printer, caf::actor>(
        sys.next_actor_id(), sys.node(), &sys, cfg);
    }

    void stop() override {
      while (run() > 0)
        trigger_timeouts();
    }

    void enqueue(caf::resumable* ptr) override {
      jobs.push_back(ptr);
      if (after_next_enqueue_ != nullptr) {
        CAF_LOG_DEBUG("inline this enqueue");
        std::function<void()> f;
        f.swap(after_next_enqueue_);
        f();
      }
    }

  private:
    void inline_all_enqueues_helper() {
      after_next_enqueue([this] { inline_all_enqueues_helper(); });
      run_once_lifo();
    }

    /// Allows users to fake time at will.
    test_actor_clock clock_;

    /// User-provided callback for triggering custom code in `enqueue`.
    std::function<void()> after_next_enqueue_;
  };

  using scheduler_type = test_coordinator_impl;

  // -- constructors, destructors, and assignment operators --------------------

  static Config& init_config(Config& cfg) {
    if (auto err = cfg.parse(caf::test::engine::argc(),
                             caf::test::engine::argv()))
      CAF_FAIL("failed to parse config: " << to_string(err));
    cfg.module_factories.push_back(
      [](caf::actor_system& sys) -> caf::actor_system::module* {
        return new scheduler_type(sys);
      });
    if (cfg.custom_options().has_category("caf.middleman")) {
      cfg.set("caf.middleman.network-backend", "testing");
      cfg.set("caf.middleman.manual-multiplexing", true);
      cfg.set("caf.middleman.workers", size_t{0});
      cfg.set("caf.middleman.heartbeat-interval", caf::timespan{0});
    }
    return cfg;
  }

  template <class... Ts>
  explicit test_coordinator_fixture(Ts&&... xs)
    : cfg(std::forward<Ts>(xs)...),
      sys(init_config(cfg)),
      self(sys, true),
      sched(dynamic_cast<scheduler_type&>(sys.scheduler())) {
    // Make sure the current time isn't 0.
    sched.clock().current_time += std::chrono::hours(1);
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
  ///          timeouts triggered.
  size_t run() {
    return run_until([] { return false; });
  }

  /// Consume ones message or triggers the next timeout.
  /// @returns `true` if a message was consumed or timeouts was triggered,
  ///          `false` otherwise.
  bool run_once() {
    return run_until([] { return true; }) > 0;
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
        ++events;
        if (predicate()) {
          CAF_LOG_DEBUG("stop due to predicate:" << CAF_ARG(events));
          return events;
        }
      }
      while (handle_io_event()) {
        ++progress;
        ++events;
        if (predicate()) {
          CAF_LOG_DEBUG("stop due to predicate:" << CAF_ARG(events));
          return events;
        }
      }
      if (trigger_timeout()) {
        ++progress;
        ++events;
      }
      if (progress == 0) {
        CAF_LOG_DEBUG("no activity left:" << CAF_ARG(events));
        return events;
      }
    }
  }

  /// Call `run()` when the next scheduled actor becomes ready.
  void run_after_next_ready_event() {
    sched.after_next_enqueue([this] { run(); });
  }

  /// Call `run_until(predicate)` when the next scheduled actor becomes ready.
  template <class BoolPredicate>
  void run_until_after_next_ready_event(BoolPredicate predicate) {
    sched.after_next_enqueue([this, predicate] { run_until(predicate); });
  }

  /// Sends a request to `hdl`, then calls `run()`, and finally fetches and
  /// returns the result.
  template <class T, class... Ts, class Handle, class... Us>
  std::conditional_t<sizeof...(Ts) == 0, T, std::tuple<T, Ts...>>
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

  template <class... Ts>
  caf::byte_buffer serialize(const Ts&... xs) {
    caf::byte_buffer buf;
    caf::binary_serializer sink{sys, buf};
    if (!(sink.apply(xs) && ...))
      CAF_FAIL("serialization failed: " << sink.get_error());
    return buf;
  }

  template <class... Ts>
  void deserialize(const caf::byte_buffer& buf, Ts&... xs) {
    caf::binary_deserializer source{sys, buf};
    if (!(source.apply(xs) && ...))
      CAF_FAIL("deserialization failed: " << source.get_error());
  }

  template <class T>
  T roundtrip(const T& x) {
    T result;
    deserialize(serialize(x), result);
    return result;
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
};

/// Unboxes an expected value or fails the test if it doesn't exist.
template <class T>
T unbox(caf::expected<T> x) {
  if (!x)
    CAF_FAIL(to_string(x.error()));
  return std::move(*x);
}

/// Unboxes an optional value or fails the test if it doesn't exist.
template <class T>
T unbox(std::optional<T> x) {
  if (!x)
    CAF_FAIL("x == std::nullopt");
  return std::move(*x);
}

/// Unboxes an optional value or fails the test if it doesn't exist.
template <class T>
T unbox(T* x) {
  if (x == nullptr)
    CAF_FAIL("x == nullptr");
  return *x;
}

/// Expands to its argument.
#define CAF_EXPAND(x) x

/// Expands to its arguments.
#define CAF_DSL_LIST(...) __VA_ARGS__

/// Convenience macro for defining expect clauses.
#define expect(types, fields)                                                  \
  (expect_clause<CAF_EXPAND(CAF_DSL_LIST types)>{sched, __LINE__}.fields.eval( \
    #types, #fields))

#define inject(types, fields)                                                  \
  (inject_clause<CAF_EXPAND(CAF_DSL_LIST types)>{sched, __LINE__}.fields.eval( \
    #types, #fields))

/// Convenience macro for defining allow clauses.
#define allow(types, fields)                                                   \
  (allow_clause<CAF_EXPAND(CAF_DSL_LIST types)>{sched, __LINE__}.fields.eval(  \
    #types, #fields))

/// Convenience macro for defining disallow clauses.
#define disallow(types, fields)                                                \
  (disallow_clause<CAF_EXPAND(CAF_DSL_LIST types)>{__LINE__}.fields.eval(      \
    #types, #fields))

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
    static inline const char* name = #tname;                                   \
  };                                                                           \
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
