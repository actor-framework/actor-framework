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

#include "caf/config.hpp"

#define CAF_SUITE aout
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

using std::endl;

namespace {

struct meta_info {
  constexpr meta_info(const char* x, size_t y, uint32_t z)
      : name(x), hash(y), version(z) {
    // nop
  }

  constexpr meta_info(const meta_info& other)
      : name(other.name),
        hash(other.hash),
        version(other.version) {
    // nop
  }

  const char* name;
  size_t hash;
  uint32_t version;
};

constexpr size_t str_hash(const char* cstr, size_t interim = 0) {
  return (*cstr == '\0') ? interim : str_hash(cstr + 1, interim * 101 + *cstr);
}

constexpr meta_info make_meta_info(const char* name, uint32_t version) {
  return meta_info{name, str_hash(name), version};
}

template <class T>
struct meta_information;

#define CAF_META_INFORMATION(class_name, version)                              \
  template <>                                                                  \
  struct meta_information<class_name> {                                        \
    static constexpr meta_info value = make_meta_info(#class_name, version);   \
  };                                                                           \
  constexpr meta_info meta_information<class_name>::value;

#define DUMMY(name)                                                            \
  class name {};                                                               \
  CAF_META_INFORMATION(name, 0)

DUMMY(foo1)
DUMMY(foo2)
DUMMY(foo3)
DUMMY(foo4)
DUMMY(foo5)
DUMMY(foo6)
DUMMY(foo7)
DUMMY(foo8)
DUMMY(foo9)
DUMMY(foo10)
DUMMY(foo11)
DUMMY(foo12)
DUMMY(foo13)
DUMMY(foo14)
DUMMY(foo15)
DUMMY(foo16)
DUMMY(foo17)
DUMMY(foo18)
DUMMY(foo19)
DUMMY(foo20)

struct fixture {
  ~fixture() {
    await_all_actors_done();
    shutdown();
  }
};


constexpr const char* global_redirect = ":test";
constexpr const char* local_redirect = ":test2";

constexpr const char* chatty_line = "hi there! :)";
constexpr const char* chattier_line = "hello there, fellow friend! :)";

void chatty_actor(event_based_actor* self) {
  aout(self) << chatty_line << endl;
}

void chattier_actor(event_based_actor* self, const std::string& fn) {
  aout(self) << chatty_line << endl;
  actor_ostream::redirect(self, fn);
  aout(self) << chattier_line << endl;
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(aout_tests, fixture)

const meta_info* lookup(const std::vector<std::pair<size_t, const meta_info*>>& haystack,
                        const meta_info* needle) {
  /*
  auto h = needle->hash;
  auto e = haystack.end();
  for (auto i = haystack.begin(); i != e; ++i) {
    if (i->first == h) {
      auto n = i + 1;
      if (n == e || n->second->hash != h)
        return i->second;
      for (auto j = i; j != e; ++j)
        if (strcmp(j->second->name, needle->name) == 0)
          return j->second;
    }
  }
  */
  auto h = needle->hash;
  auto e = haystack.end();
  //auto i = std::find_if(haystack.begin(), e,
  //                      [h](const std::pair<size_t, const meta_info*>& x ) { return x.first == h; });
  auto i = std::lower_bound(haystack.begin(), e, h,
                        [](const std::pair<size_t, const meta_info*>& x, size_t y) { return x.first < y; });
  if (i == e || i->first != h)
    return nullptr;
  // check for collision
  auto n = i + 1;
  if (n == e || n->first != h)
    return i->second;
  i = std::find_if(i, e, [needle](const std::pair<size_t, const meta_info*>& x ) { return strcmp(x.second->name, needle->name) == 0; });
  if (i != e)
    return i->second;
  return nullptr;
}

const meta_info* lookup(std::unordered_multimap<size_t, const meta_info*>& haystack,
                        const meta_info* needle) {
  auto i = haystack.find(needle->hash);
  auto e = haystack.end();
  if (i == e)
    return nullptr;
  auto n = std::next(i);
  if (n == e || n->second->hash != i->second->hash)
    return i->second;
  for (; i != e; ++i)
    if (strcmp(i->second->name, needle->name) == 0)
      return i->second;
  return nullptr;
}

const meta_info* lookup(std::multimap<size_t, const meta_info*>& haystack,
                        const meta_info* needle) {
  auto i = haystack.find(needle->hash);
  auto e = haystack.end();
  if (i == e)
    return nullptr;
  auto n = std::next(i);
  if (n == e || n->second->hash != i->second->hash)
    return i->second;
  for (; i != e; ++i)
    if (strcmp(i->second->name, needle->name) == 0)
      return i->second;
  return nullptr;
}

CAF_TEST(foobar) {
  using std::make_pair;
  std::vector<std::pair<size_t, const meta_info*>> map1;
  std::unordered_multimap<size_t, const meta_info*> map2;
  std::multimap<size_t, const meta_info*> map3;
  const meta_info* arr[] = {
    &meta_information<foo1>::value,
    &meta_information<foo2>::value,
    &meta_information<foo3>::value,
    &meta_information<foo4>::value,
    &meta_information<foo5>::value,
    &meta_information<foo6>::value,
    &meta_information<foo7>::value,
    &meta_information<foo8>::value,
    &meta_information<foo9>::value,
    &meta_information<foo10>::value,
    &meta_information<foo11>::value,
    &meta_information<foo12>::value,
    &meta_information<foo13>::value,
    &meta_information<foo14>::value,
    &meta_information<foo15>::value,
    &meta_information<foo16>::value,
    &meta_information<foo17>::value,
    &meta_information<foo18>::value,
    &meta_information<foo19>::value,
    &meta_information<foo20>::value
  };
  for (auto i = std::begin(arr); i != std::end(arr); ++i) {
    map1.emplace_back((*i)->hash, *i);
    map2.emplace((*i)->hash, *i);
    map3.emplace((*i)->hash, *i);
  }
  std::sort(map1.begin(), map1.end());
  std::array<const meta_info*, 20> dummy;
  {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < 10000; ++i)
      dummy[i % 20] = lookup(map1, arr[i % 20]);
    auto t1 = std::chrono::high_resolution_clock::now();
    std::cout << "vector: " << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() << std::endl;
    std::cout << "check: " << std::equal(dummy.begin(), dummy.end(), std::begin(arr)) << std::endl;
  }
  {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < 10000; ++i)
      dummy[i % 20] = lookup(map2, arr[i % 20]);
    auto t1 = std::chrono::high_resolution_clock::now();
    std::cout << "hash map: " << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() << std::endl;
    std::cout << "check: " << std::equal(dummy.begin(), dummy.end(), std::begin(arr)) << std::endl;
  }
  {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < 10000; ++i)
      dummy[i % 20] = lookup(map3, arr[i % 20]);
    auto t1 = std::chrono::high_resolution_clock::now();
    std::cout << "map: " << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() << std::endl;
    std::cout << "check: " << std::equal(dummy.begin(), dummy.end(), std::begin(arr)) << std::endl;
  }
  CAF_CHECK(meta_information<foo1>::value.name == "foo1");
  CAF_CHECK(meta_information<foo1>::value.hash == str_hash("foo1"));
  CAF_CHECK(meta_information<foo1>::value.version == 0);
}

CAF_TEST(global_redirect) {
  scoped_actor self;
  self->join(group::get("local", global_redirect));
  actor_ostream::redirect_all(global_redirect);
  spawn(chatty_actor);
  self->receive(
    [](const std::string& virtual_file, std::string& line) {
      // drop trailing '\n'
      if (! line.empty())
        line.pop_back();
      CAF_CHECK_EQUAL(virtual_file, ":test");
      CAF_CHECK_EQUAL(line, chatty_line);
    }
  );
  self->await_all_other_actors_done();
  CAF_CHECK_EQUAL(self->mailbox().count(), 0);
}

CAF_TEST(global_and_local_redirect) {
  scoped_actor self;
  self->join(group::get("local", global_redirect));
  self->join(group::get("local", local_redirect));
  actor_ostream::redirect_all(global_redirect);
  spawn(chatty_actor);
  spawn(chattier_actor, local_redirect);
  std::vector<std::pair<std::string, std::string>> expected {
    {":test", chatty_line},
    {":test", chatty_line},
    {":test2", chattier_line}
  };
  std::vector<std::pair<std::string, std::string>> lines;
  int i = 0;
  self->receive_for(i, 3)(
    [&](std::string& virtual_file, std::string& line) {
      // drop trailing '\n'
      if (! line.empty())
        line.pop_back();
      lines.emplace_back(std::move(virtual_file), std::move(line));
    }
  );
  CAF_CHECK(std::is_permutation(lines.begin(), lines.end(), expected.begin()));
  self->await_all_other_actors_done();
  CAF_CHECK_EQUAL(self->mailbox().count(), 0);
}

CAF_TEST_FIXTURE_SCOPE_END()
