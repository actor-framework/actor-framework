// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.monotonic_buffer_resource

#include "caf/detail/monotonic_buffer_resource.hpp"

#include "core-test.hpp"

#include <list>
#include <map>
#include <vector>

using namespace caf;

SCENARIO("monotonic buffers group allocations") {
  GIVEN("a monotonic buffer resource") {
    detail::monotonic_buffer_resource mbr;
    WHEN("calling allocate multiple times for the same size") {
      THEN("the resource returns consecutive pointers") {
        CHECK_EQ(mbr.blocks(8), 0u);
        auto p1 = mbr.allocate(8);
        auto p2 = mbr.allocate(8);
        auto p3 = mbr.allocate(8);
        CHECK_EQ(mbr.blocks(8), 1u);
        CHECK(p1 < p2);
        CHECK(p2 < p3);
      }
    }
  }
  GIVEN("a monotonic buffer resource") {
    detail::monotonic_buffer_resource mbr;
    WHEN("calling allocate with various sizes") {
      THEN("the resource puts allocations into buckets") {
        void* unused = nullptr; // For silencing nodiscard warnings.
        CHECK_EQ(mbr.blocks(), 0u);
        MESSAGE("perform small allocations");
        unused = mbr.allocate(64);
        CHECK_EQ(mbr.blocks(), 1u);
        unused = mbr.allocate(64);
        CHECK_EQ(mbr.blocks(), 1u);
        MESSAGE("perform medium allocations");
        unused = mbr.allocate(65);
        CHECK_EQ(mbr.blocks(), 2u);
        unused = mbr.allocate(512);
        CHECK_EQ(mbr.blocks(), 2u);
        MESSAGE("perform large allocations <= 1 MB (pools allocations)");
        unused = mbr.allocate(513);
        CHECK_EQ(mbr.blocks(), 3u);
        unused = mbr.allocate(1023);
        CHECK_EQ(mbr.blocks(), 3u);
        MESSAGE("perform large allocations  > 1 MB (allocates individually)");
        unused = mbr.allocate(1'048'577);
        CHECK_EQ(mbr.blocks(), 4u);
        unused = mbr.allocate(1'048'577);
        CHECK_EQ(mbr.blocks(), 5u);
      }
    }
  }
}

SCENARIO("monotonic buffers re-use memory after calling reclaim") {
  std::vector<void*> locations;
  GIVEN("a monotonic buffer resource with some allocations performed on it") {
    detail::monotonic_buffer_resource mbr;
    locations.push_back(mbr.allocate(64));
    locations.push_back(mbr.allocate(64));
    locations.push_back(mbr.allocate(65));
    locations.push_back(mbr.allocate(512));
    locations.push_back(mbr.allocate(513));
    locations.push_back(mbr.allocate(1023));
    locations.push_back(mbr.allocate(1'048'577));
    locations.push_back(mbr.allocate(1'048'577));
    WHEN("calling reclaim on the resource") {
      mbr.reclaim();
      THEN("performing the same allocations returns the same addresses again") {
        if (CHECK_EQ(mbr.blocks(), 5u)) {
          CHECK_EQ(locations[0], mbr.allocate(64));
          CHECK_EQ(locations[1], mbr.allocate(64));
          CHECK_EQ(locations[2], mbr.allocate(65));
          CHECK_EQ(locations[3], mbr.allocate(512));
          CHECK_EQ(locations[4], mbr.allocate(513));
          CHECK_EQ(locations[5], mbr.allocate(1023));
          CHECK_EQ(locations[6], mbr.allocate(1'048'577));
          CHECK_EQ(locations[7], mbr.allocate(1'048'577));
          CHECK_EQ(mbr.blocks(), 5u);
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
        CHECK_EQ(mbr.blocks(), 0u);
        std::vector<int32_t, int_allocator> xs{int_allocator{&mbr}};
        xs.push_back(42);
        CHECK_EQ(xs.size(), 1u);
        CHECK_EQ(mbr.blocks(), 1u);
        xs.insert(xs.end(), 17, 0);
      }
    }
  }
  GIVEN("a monotonic buffer resource and a std::list") {
    using int_allocator = detail::monotonic_buffer_resource::allocator<int32_t>;
    detail::monotonic_buffer_resource mbr;
    WHEN("pushing to the list") {
      THEN("the memory resource fills up") {
        CHECK_EQ(mbr.blocks(), 0u);
        std::list<int32_t, int_allocator> xs{int_allocator{&mbr}};
        xs.push_back(42);
        CHECK_EQ(xs.size(), 1u);
        CHECK_EQ(mbr.blocks(), 1u);
      }
    }
  }
}
