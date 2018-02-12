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

#include "caf/all.hpp"
#include "caf/meta/annotation.hpp"
#include "caf/test/unit_test.hpp"

namespace {

struct wildcard { };

constexpr wildcard _ = wildcard{};

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

} // namespace <anonymous>

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

namespace {

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
T get(const U&);

// enables ADL in `with_content`
template <class T, class U>
bool is(const U&);

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
class caf_handle {
public:
  using pointer = caf::abstract_actor*;

  template <class T>
  caf_handle(const T& x) {
    *this = x;
  }

  caf_handle& operator=(caf::abstract_actor* x)  {
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

private:
  caf_handle() : ptr_(nullptr) {
    // nop
  }

  caf::abstract_actor* ptr_;
};

// -- access to an actor's mailbox ---------------------------------------------

/// Returns a pointer to the next element in an actor's mailbox without taking
/// it out of the mailbox.
/// @pre `ptr` is alive and either a `scheduled_actor` or `blocking_actor`
caf::mailbox_element* next_mailbox_element(caf_handle x) {
  CAF_ASSERT(x.get() != nullptr);
  auto sptr = dynamic_cast<caf::scheduled_actor*>(x.get());
  if (sptr != nullptr)
    return sptr->mailbox().peek();
  auto bptr = dynamic_cast<caf::blocking_actor*>(x.get());
  CAF_ASSERT(bptr != nullptr);
  return bptr->mailbox().peek();
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
struct extract_impl {
  caf::optional<std::tuple<T, Ts...>> operator()(caf_handle x) {
    return default_extract<T, Ts...>(x);
  }
};

template <class T>
struct extract_impl<T, true> {
  caf::optional<std::tuple<T>> operator()(caf_handle x) {
    return unboxing_extract<T>(x);
  }
};

/// Returns the content of the next mailbox element without taking it out of
/// the mailbox. Fails on an empty mailbox or if the content of the next
/// element does not match `<T, Ts...>`.
template <class T, class... Ts>
std::tuple<T, Ts...> extract(caf_handle x) {
  extract_impl<T, has_outer_type<T>::value, Ts...> f;
  auto result = f(x);
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
  extract_impl<T, has_outer_type<T>::value, Ts...> f;
  return f(x) != caf::none;
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
      CAF_CHECK_EQUAL(ptr->sender, src_);
    return *this;
  }

  expect_clause& to(const caf::scoped_actor& whom) {
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
      auto ys = extract<Ts...>(dest_);
      auto ys_indices = get_indices(ys);
      CAF_CHECK(apply_args(inspector, ys_indices, ys));
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

template <class Derived>
class disallow_clause_base {
public:
  disallow_clause_base(caf::scheduler::test_coordinator& sched)
      : sched_(sched),
        dest_(nullptr) {
    // nop
  }

  disallow_clause_base(disallow_clause_base&& other)
      : sched_(other.sched_),
        src_(std::move(other.src_)) {
    // nop
  }

  Derived& from(const wildcard&) {
    return dref();
  }

  template <class Handle>
  Derived& from(const Handle& whom) {
    src_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return dref();
  }

  template <class Handle>
  Derived& to(const Handle& whom) {
    // not setting dest_ causes the content checking to succeed immediately
    if (sched_.prioritize(whom)) {
      dest_ = &sched_.next_job<caf::scheduled_actor>();
    }
    return dref();
  }

  Derived& to(const wildcard& whom) {
    if (sched_.prioritize(whom))
      dest_ = &sched_.next_job<caf::scheduled_actor>();
  }

  Derived& to(const caf::scoped_actor& whom) {
    dest_ = whom.ptr();
    return dref();
  }

  template <class... Ts>
  caf::optional<std::tuple<const Ts&...>> peek() {
    CAF_REQUIRE(dest_ != nullptr);
    auto ptr = peek(dest_);
    if (!ptr->content().template match_elements<Ts...>())
      return caf::none;
    return ptr->content().template get_as_tuple<Ts...>();
  }

protected:
  Derived& dref() {
    return *static_cast<Derived*>(this);
  }

  caf::scheduler::test_coordinator& sched_;
  caf::strong_actor_ptr src_;
  caf::local_actor* dest_;
};

template <class... Ts>
class disallow_clause : public disallow_clause_base<disallow_clause<Ts...>> {
public:
  template <class... Us>
  disallow_clause(Us&&... xs)
      : disallow_clause_base<disallow_clause<Ts...>>(std::forward<Us>(xs)...) {
    // nop
  }

  template <class... Us>
  void with(Us&&... xs) {
    // succeed immediately if dest_ is empty
    if (this->dest_ == nullptr)
      return;
    auto tmp = std::make_tuple(std::forward<Us>(xs)...);
    elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
    auto ys = this->template peek<Ts...>();
    if (ys && inspector(get<0>(*ys)))
      CAF_FAIL("disallowed message found: " << caf::deep_to_string(ys));
  }
};

/// The single-argument disallow-clause allows to automagically unwrap T
/// if it's a variant-like wrapper.
template <class T>
class disallow_clause<T> : public disallow_clause_base<disallow_clause<T>> {
public:
  template <class... Us>
  disallow_clause(Us&&... xs)
      : disallow_clause_base<disallow_clause<T>>(std::forward<Us>(xs)...) {
    // nop
  }

  template <class... Us>
  void with(Us&&... xs) {
    if (this->dest_ == nullptr)
      return;
    std::integral_constant<bool, has_outer_type<T>::value> token;
    auto tmp = std::make_tuple(std::forward<Us>(xs)...);
    with_content(token, tmp);
  }

private:
  template <class U>
  void with_content(std::integral_constant<bool, false>, const U& x) {
    elementwise_compare_inspector<U> inspector{x};
    auto xs = this->template peek<T>();
    if (xs && inspector(get<0>(*xs)))
      CAF_FAIL("disallowed message found: " << caf::deep_to_string(*xs));
  }

  template <class U>
  void with_content(std::integral_constant<bool, true>, const U& x) {
    elementwise_compare_inspector<U> inspector{x};
    auto xs = this->template peek<typename T::outer_type>();
    if (!xs)
      return;
    auto& x0 = get<0>(*xs);
    if (is<T>(x0) && inspect(inspector, const_cast<T&>(get<T>(x0))))
      CAF_FAIL("disallowed message found: " << caf::deep_to_string(x0));
  }

};

template <>
class disallow_clause<void>
  : public disallow_clause_base<disallow_clause<void>> {
public:
  template <class... Us>
  disallow_clause(Us&&... xs)
      : disallow_clause_base<disallow_clause<void>>(std::forward<Us>(xs)...) {
    // nop
  }

  void with() {
    if (dest_ == nullptr)
      return;
    auto ptr = next_mailbox_element(dest_);
    CAF_REQUIRE(!ptr->content().empty());
  }
};

template <class Config = caf::actor_system_config>
struct test_coordinator_fixture {
  using scheduler_type = caf::scheduler::test_coordinator;

  Config cfg;
  caf::actor_system sys;
  caf::scoped_actor self;
  scheduler_type& sched;

  test_coordinator_fixture()
      : sys(cfg.parse(caf::test::engine::argc(), caf::test::engine::argv())
               .set("scheduler.policy", caf::atom("testing"))),
        self(sys),
        sched(dynamic_cast<scheduler_type&>(sys.scheduler())) {
    // nop
  }

  ~test_coordinator_fixture() {
    sched.clock().cancel_all();
  }

  template <class T = int>
  caf::expected<T> fetch_result() {
    caf::expected<T> result = caf::error{};
    self->receive(
      [&](T& x) {
        result = std::move(x);
      },
      [&](caf::error& x) {
        result = std::move(x);
      },
      caf::after(std::chrono::seconds(0)) >> [&] {
        result = caf::sec::request_timeout;
      }
    );
    return result;
  }

  template <class T>
  const T& peek() {
    return sched.template peek<T>();
  }

  template <class T = caf::scheduled_actor, class Handle = caf::actor>
  T& deref(const Handle& hdl) {
    auto ptr = caf::actor_cast<caf::abstract_actor*>(hdl);
    CAF_REQUIRE(ptr != nullptr);
    return dynamic_cast<T&>(*ptr);
  }
};

} // namespace <anonymous>

#define CAF_EXPAND(x) x
#define CAF_DSL_LIST(...) __VA_ARGS__

#define expect(types, fields)                                                  \
  CAF_MESSAGE("expect" << #types << "." << #fields);                           \
  expect_clause< CAF_EXPAND(CAF_DSL_LIST types) >{sched} . fields

#define disallow(types, fields)                                                \
  CAF_MESSAGE("disallow" << #types << "." << #fields);                         \
  disallow_clause< CAF_EXPAND(CAF_DSL_LIST types) >{sched} . fields
