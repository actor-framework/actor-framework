// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/linked_string_chunk.hpp"

#include "caf/test/test.hpp"

#include "caf/detail/monotonic_buffer_resource.hpp"

using namespace caf;
using namespace std::literals;

namespace {

TEST("to_string") {
  auto world = linked_string_chunk{"World!", nullptr};
  check_eq(to_string(world), "World!");
  auto hello = linked_string_chunk{"Hello, ", &world};
  check_eq(to_string(hello), "Hello, World!");
}

TEST("format") {
  auto world = linked_string_chunk{"World!", nullptr};
  check_eq(detail::format("{}", world), "World!");
  check_eq(detail::format("{:?}", world), R"_("World!")_");
  check_eq(detail::format("{:?} {}", world, world), R"_("World!" World!)_");
  auto hello = linked_string_chunk{"Hello, ", &world};
  check_eq(detail::format("{}", hello), "Hello, World!");
  check_eq(detail::format("{:?}", hello), R"_("Hello, World!")_");
  auto prefix = linked_string_chunk{"\"Jon\":\t", &hello};
  check_eq(detail::format("{}", prefix), "\"Jon\":\tHello, World!");
  check_eq(detail::format("{:?}", prefix), R"_("\"Jon\":\tHello, World!")_");
}

TEST("builder") {
  auto resource = detail::monotonic_buffer_resource{};
  auto builder = linked_string_chunk_builder{&resource};
  auto out = linked_string_chunk_builder_output_iterator{&builder};
  SECTION("empty") {
    check_eq(to_string(*builder.seal()), "");
  }
  SECTION("iterator interface") {
    out = 'h';
    ++out;
    out = 'e';
    out++;
    *out++ = 'l';
    *out++ = 'l';
    *out = 'o';
    check_eq(to_string(*builder.seal()), "hello");
  }
  SECTION("single chunk") {
    auto hello = "Hello, World!"s;
    std::copy(hello.begin(), hello.end(), out);
    check_eq(to_string(*builder.seal()), hello);
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
    auto head = builder.seal();
    check_eq(to_string(*head), str);
    auto sizes = std::vector<size_t>{};
    for (auto chunk = head; chunk != nullptr; chunk = chunk->next)
      sizes.push_back(chunk->content.size());
    check_eq(sizes, std::vector<size_t>{128, 128, 128, 128, 1});
  }
}

} // namespace
