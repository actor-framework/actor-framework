// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/subscription.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/test.hpp"

#include <limits>
#include <vector>

using namespace caf;

namespace {

class recording_impl : public test::fixture::flow::counting_sub {
public:
  using counting_sub::counting_sub;

  void request(size_t n) override {
    requests.push_back(n);
  }

  std::vector<size_t> requests;
};

} // namespace

WITH_FIXTURE(test::fixture::flow) {

TEST("subscription::request ignores zero demand") {
  auto impl = coordinator()->add_child(std::in_place_type<recording_impl>);
  auto uut = caf::flow::subscription{impl};
  uut.request(0);
  check(impl->requests.empty());
}

TEST("subscription::request forwards positive demand unchanged") {
  auto impl = coordinator()->add_child(std::in_place_type<recording_impl>);
  auto uut = caf::flow::subscription{impl};
  uut.request(1);
  uut.request(0);
  uut.request(7);
  uut.request(0);
  check_eq(impl->requests, std::vector<size_t>({1, 7}));
}

TEST("subscription::request accepts the largest representable demand") {
  auto impl = coordinator()->add_child(std::in_place_type<recording_impl>);
  auto uut = caf::flow::subscription{impl};
  auto n = std::numeric_limits<size_t>::max();
  uut.request(n);
  check_eq(impl->requests, std::vector<size_t>{n});
}

} // WITH_FIXTURE(test::fixture::flow)
