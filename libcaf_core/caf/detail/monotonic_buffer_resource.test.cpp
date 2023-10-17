// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/monotonic_buffer_resource.hpp"

#include "caf/test/scenario.hpp"

#include <list>
#include <map>
#include <vector>

using namespace caf;

namespace {

SCENARIO("monotonic buffers group allocations") {
  GIVEN("a monotonic buffer resource") {
    detail::monotonic_buffer_resource mbr;
    WHEN("calling allocate multiple times for the same size") {
      THEN("the resource returns consecutive pointers") {
        check_eq(mbr.blocks(8), 0u);
        auto p1 = mbr.allocate(8);
        auto p2 = mbr.allocate(8);
        auto p3 = mbr.allocate(8);
        check_eq(mbr.blocks(8), 1u);
        check(p1 < p2);
        check(p2 < p3);
      }
    }
  }
  GIVEN("a monotonic buffer resource") {
    detail::monotonic_buffer_resource mbr;
    WHEN("calling allocate with various sizes") {
      THEN("the resource puts allocations into buckets") {
        void* unused = nullptr; // For silencing nodiscard warnings.
        check_eq(mbr.blocks(), 0u);
        print_debug("perform small allocations");
        unused = mbr.allocate(64);
        check_eq(mbr.blocks(), 1u);
        unused = mbr.allocate(64);
        check_eq(mbr.blocks(), 1u);
        print_debug("perform medium allocations");
        unused = mbr.allocate(65);
        check_eq(mbr.blocks(), 2u);
        unused = mbr.allocate(512);
        check_eq(mbr.blocks(), 2u);
        print_debug("perform large allocations <= 1 MB (pools allocations)");
        unused = mbr.allocate(513);
        check_eq(mbr.blocks(), 3u);
        unused = mbr.allocate(1023);
        check_eq(mbr.blocks(), 3u);
        print_debug("perform large allocations > 1 MB (individual allocation)");
        unused = mbr.allocate(1'048'577);
        check_eq(mbr.blocks(), 4u);
        unused = mbr.allocate(1'048'577);
        check_eq(mbr.blocks(), 5u);
        static_cast<void>(unused);
      }
    }
  }
}

SCENARIO("monotonic buffers re-use small memory blocks after calling reclaim") {
  std::vector<void*> locations;
  GIVEN("a monotonic buffer resource with some allocations performed on it") {
    detail::monotonic_buffer_resource mbr;
    locations.push_back(mbr.allocate(64));
    locations.push_back(mbr.allocate(64));
    locations.push_back(mbr.allocate(65));
    locations.push_back(mbr.allocate(512));
    WHEN("calling reclaim on the resource") {
      mbr.reclaim();
      THEN("performing the same allocations returns the same addresses again") {
        if (check_eq(mbr.blocks(), 2u)) {
          check_eq(locations[0], mbr.allocate(64));
          check_eq(locations[1], mbr.allocate(64));
          check_eq(locations[2], mbr.allocate(65));
          check_eq(locations[3], mbr.allocate(512));
          check_eq(mbr.blocks(), 2u);
        }
      }
    }
  }
}

SCENARIO("monotonic buffers provide storage for STL containers") {
  GIVEN("a monotonic buffer resource and a std::vector") {
    using int_allocator = detail::monotonic_buffer_resource::allocator<int32_t>;
    detail::monotonic_buffer_resource mbr;
    WHEN("pushing to the vector") {
      THEN("the memory resource fills up") {
        check_eq(mbr.blocks(), 0u);
        std::vector<int32_t, int_allocator> xs{int_allocator{&mbr}};
        xs.push_back(42);
        check_eq(xs.size(), 1u);
        check_eq(mbr.blocks(), 1u);
        xs.insert(xs.end(), 17, 0);
      }
    }
  }
  GIVEN("a monotonic buffer resource and a std::list") {
    using int_allocator = detail::monotonic_buffer_resource::allocator<int32_t>;
    detail::monotonic_buffer_resource mbr;
    WHEN("pushing to the list") {
      THEN("the memory resource fills up") {
        check_eq(mbr.blocks(), 0u);
        std::list<int32_t, int_allocator> xs{int_allocator{&mbr}};
        xs.push_back(42);
        check_eq(xs.size(), 1u);
        check_eq(mbr.blocks(), 1u);
      }
    }
  }
}

} // namespace
