/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

// allow comparing arbitrary `T`s to `message` objects
namespace caf {
template <class T>
bool operator==(const message& x, const T& y) {
  return x.match_elements<T>() && x.get_as<T>(0) == y;
}
} // namespace caf

namespace {

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

struct wildcard { };

static constexpr wildcard _ = wildcard{};

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

template <class T>
class expect_clause {
public:
  expect_clause(caf::scheduler::test_coordinator& sched) : sched_(sched) {
    // nop
  }

  expect_clause(expect_clause&& other)
      : sched_(other.sched_),
        src_(std::move(other.src_)) {
    // nop
  }

  template <class Handle>
  expect_clause& from(const Handle& whom) {
    src_ = caf::actor_cast<caf::strong_actor_ptr>(whom);
    return *this;
  }

  template <class Handle>
  expect_clause& to(const Handle& whom) {
    CAF_REQUIRE(sched_.prioritize(whom));
    auto ptr = sched_.next_job<caf::scheduled_actor>().mailbox().peek();
    CAF_REQUIRE(ptr != nullptr);
    CAF_REQUIRE_EQUAL(ptr->sender, src_);
    return *this;
  }

  template <class... Ts>
  void with(Ts&&... xs) {
    std::integral_constant<bool, has_outer_type<T>::value> token;
    auto tmp = std::make_tuple(std::forward<Ts>(xs)...);
    with_content(token, tmp);
    sched_.run_once();
  }

private:
  template <class U>
  void with_content(std::integral_constant<bool, false>, const U& x) {
    elementwise_compare_inspector<U> inspector{x};
    CAF_CHECK(inspector(const_cast<T&>(sched_.peek<T>())));
  }

  template <class U>
  void with_content(std::integral_constant<bool, true>, const U& x) {
    elementwise_compare_inspector<U> inspector{x};
    auto& y = sched_.peek<typename T::outer_type>();
    CAF_CHECK(inspector(const_cast<T&>(get<T>(y))));
  }

  caf::scheduler::test_coordinator& sched_;
  caf::strong_actor_ptr src_;
};

template <class Config = caf::actor_system_config>
struct test_coordinator_fixture {
  using scheduler_type = caf::scheduler::test_coordinator;

  Config cfg;
  caf::actor_system sys;
  caf::scoped_actor self;
  scheduler_type& sched;

  test_coordinator_fixture()
      : sys((cfg.scheduler_policy = caf::atom("testing"), cfg)),
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

  template <class T>
    expect_clause<T> expect() {
      return {sched};
    }
};

} // namespace <anonymous>

#define expect(type, fields)                                                   \
  CAF_MESSAGE("expect(" << #type << ")." << #fields);                          \
  expect<type>().fields

