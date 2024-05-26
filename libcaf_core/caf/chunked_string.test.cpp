// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/chunked_string.hpp"

#include "caf/test/test.hpp"

#include "caf/detail/monotonic_buffer_resource.hpp"

using namespace caf;
using namespace std::literals;

namespace {

using list_type = detail::mbr_list<std::string_view>;

using node_type = list_type::node_type;

auto str(const node_type& head) {
  return chunked_string{&head};
}

TEST("to_string") {
  auto world = node_type{"World!", nullptr};
  check_eq(to_string(str(world)), "World!");
  auto hello = node_type{"Hello, ", &world};
  check_eq(to_string(str(hello)), "Hello, World!");
}

TEST("format") {
  auto world = node_type{"World!", nullptr};
  check_eq(detail::format("{}", str(world)), "World!");
  check_eq(detail::format("{:?}", str(world)), R"_("World!")_");
  check_eq(detail::format("{:?} {}", str(world), str(world)),
           R"_("World!" World!)_");
  auto hello = node_type{"Hello, ", &world};
  check_eq(detail::format("{}", str(hello)), "Hello, World!");
  check_eq(detail::format("{:?}", str(hello)), R"_("Hello, World!")_");
  auto prefix = node_type{"\"Jon\":\t", &hello};
  check_eq(detail::format("{}", str(prefix)), "\"Jon\":\tHello, World!");
  check_eq(detail::format("{:?}", str(prefix)),
           R"_("\"Jon\":\tHello, World!")_");
}

TEST("builder") {
  auto resource = detail::monotonic_buffer_resource{};
  auto builder = chunked_string_builder{&resource};
  auto out = chunked_string_builder_output_iterator{&builder};
  SECTION("empty") {
    check_eq(to_string(builder.build()), "");
  }
  SECTION("iterator interface") {
    out = 'h';
    ++out;
    out = 'e';
    out++;
    *out++ = 'l';
    *out++ = 'l';
    *out = 'o';
    check_eq(to_string(builder.build()), "hello");
  }
  SECTION("single chunk") {
    auto hello = "Hello, World!"s;
    std::copy(hello.begin(), hello.end(), out);
    check_eq(to_string(builder.build()), hello);
  }
  SECTION("multiple chunks") {
    auto next = [ch = '1']() mutable {
      auto res = ch;
      if (ch == '9')
        ch = '0';
      else
        ++ch;
      return res;
    };
    auto str = std::string{};
    str.reserve(513);
    for (size_t i = 0; i < 513; ++i)
      str += next();
    std::copy(str.begin(), str.end(), out);
    auto res = builder.build();
    check_eq(to_string(res), str);
    auto sizes = std::vector<size_t>{};
    for (auto chunk : res)
      sizes.push_back(chunk.size());
    check_eq(sizes, std::vector<size_t>{128, 128, 128, 128, 1});
  }
}

} // namespace
