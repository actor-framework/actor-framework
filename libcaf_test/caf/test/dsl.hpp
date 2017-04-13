/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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
  return msg_cmp_rec<0>(x, y);
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

template <class Derived>
class expect_clause_base {
public:
  expect_clause_base(caf::scheduler::test_coordinator& sched)
      : sched_(sched),
        mock_dest_(false),
        dest_(nullptr) {
    // nop
  }

  expect_clause_base(expect_clause_base&& other)
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
    CAF_REQUIRE(sched_.prioritize(whom));
    dest_ = &sched_.next_job<caf::scheduled_actor>();
    auto ptr = dest_->mailbox().peek();
    CAF_REQUIRE(ptr != nullptr);
    if (src_)
      CAF_REQUIRE_EQUAL(ptr->sender, src_);
    return dref();
  }

  Derived& to(const wildcard& whom) {
    CAF_REQUIRE(sched_.prioritize(whom));
    dest_ = &sched_.next_job<caf::scheduled_actor>();
    return dref();
  }

  Derived& to(const caf::scoped_actor& whom) {
    mock_dest_ = true;
    dest_ = whom.ptr();
    return dref();
  }

  template <class... Ts>
  std::tuple<const Ts&...> peek() {
    CAF_REQUIRE(dest_ != nullptr);
    auto ptr = dest_->mailbox().peek();
    if (!ptr->content().match_elements<Ts...>()) {
      CAF_FAIL("Message does not match expected pattern: " << to_string(ptr->content()));
    }
    //CAF_REQUIRE(ptr->content().match_elements<Ts...>());
    return ptr->content().get_as_tuple<Ts...>();
  }

protected:
  void run_once() {
    if (dynamic_cast<caf::blocking_actor*>(dest_) == nullptr)
      sched_.run_once();
    else // remove message from mailbox
      delete dest_->mailbox().try_pop();
  }

  Derived& dref() {
    return *static_cast<Derived*>(this);
  }

  // denotes whether destination is a mock actor, i.e., a scoped_actor without
  // functionality other than checking outputs of other actors
  caf::scheduler::test_coordinator& sched_;
  bool mock_dest_;
  caf::strong_actor_ptr src_;
  caf::local_actor* dest_;
};

template <class... Ts>
class expect_clause : public expect_clause_base<expect_clause<Ts...>> {
public:
  template <class... Us>
  expect_clause(Us&&... xs)
      : expect_clause_base<expect_clause<Ts...>>(std::forward<Us>(xs)...) {
    // nop
  }

  template <class... Us>
  void with(Us&&... xs) {
    auto tmp = std::make_tuple(std::forward<Us>(xs)...);
    elementwise_compare_inspector<decltype(tmp)> inspector{tmp};
    auto ys = this->template peek<Ts...>();
    CAF_CHECK(inspector(get<0>(ys)));
    this->run_once();
  }
};

/// The single-argument expect-clause allows to automagically unwrap T
/// if it's a variant-like wrapper.
template <class T>
class expect_clause<T> : public expect_clause_base<expect_clause<T>> {
public:
  template <class... Us>
  expect_clause(Us&&... xs)
      : expect_clause_base<expect_clause<T>>(std::forward<Us>(xs)...) {
    // nop
  }

  template <class... Us>
  void with(Us&&... xs) {
    std::integral_constant<bool, has_outer_type<T>::value> token;
    auto tmp = std::make_tuple(std::forward<Us>(xs)...);
    with_content(token, tmp);
    this->run_once();
  }

private:
  template <class U>
  void with_content(std::integral_constant<bool, false>, const U& x) {
    elementwise_compare_inspector<U> inspector{x};
    auto xs = this->template peek<T>();
    CAF_CHECK(inspector(get<0>(xs)));
  }

  template <class U>
  void with_content(std::integral_constant<bool, true>, const U& x) {
    elementwise_compare_inspector<U> inspector{x};
    auto xs = this->template peek<typename T::outer_type>();
    auto& x0 = get<0>(xs);
    if (!is<T>(x0)) {
      CAF_FAIL("is<T>(x0) !! " << caf::deep_to_string(x0));
    }
    CAF_CHECK(inspect(inspector, const_cast<T&>(get<T>(x0))));
  }

};

template <>
class expect_clause<void> : public expect_clause_base<expect_clause<void>> {
public:
  template <class... Us>
  expect_clause(Us&&... xs)
      : expect_clause_base<expect_clause<void>>(std::forward<Us>(xs)...) {
    // nop
  }

  void with() {
    CAF_REQUIRE(dest_ != nullptr);
    auto ptr = dest_->mailbox().peek();
    CAF_CHECK(ptr->content().empty());
    this->run_once();
  }
};

template <class Derived>
class disallow_clause_base {
public:
  disallow_clause_base(caf::scheduler::test_coordinator& sched)
      : sched_(sched),
        mock_dest_(false),
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
    // not setting dest_ causes the the content checking to succeed immediately
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
    mock_dest_ = true;
    dest_ = whom.ptr();
    return dref();
  }

  template <class... Ts>
  caf::optional<std::tuple<const Ts&...>> peek() {
    CAF_REQUIRE(dest_ != nullptr);
    auto ptr = dest_->mailbox().peek();
    if (!ptr->content().match_elements<Ts...>())
      return caf::none;
    return ptr->content().get_as_tuple<Ts...>();
  }

protected:
  Derived& dref() {
    return *static_cast<Derived*>(this);
  }

  // denotes whether destination is a mock actor, i.e., a scoped_actor without
  // functionality other than checking outputs of other actors
  caf::scheduler::test_coordinator& sched_;
  bool mock_dest_;
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
    auto ptr = dest_->mailbox().peek();
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
      : sys(cfg.set("scheduler.policy", caf::atom("testing"))),
        self(sys),
        sched(dynamic_cast<scheduler_type&>(sys.scheduler())) {
    //sys.await_actors_before_shutdown(false);
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

  template <class... Ts>
  expect_clause<Ts...> expect_impl() {
    return {sched};
  }

  template <class... Ts>
  disallow_clause<Ts...> disallow_impl() {
    return {sched};
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
