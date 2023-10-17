// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/ieee_754.hpp"

#include "caf/test/test.hpp"

#include <limits>

using caf::detail::pack754;
using caf::detail::unpack754;

namespace {

template <class T>
T roundtrip(T x) {
  return unpack754(pack754(x));
}

using flimits = std::numeric_limits<float>;

using dlimits = std::numeric_limits<double>;

#define CHECK_RT(value) check_eq(roundtrip(value), value)

#define CHECK_PRED_RT(pred, value) check(pred(roundtrip(value)))

#define CHECK_SIGN_RT(value)                                                   \
  check_eq(std::signbit(roundtrip(value)), std::signbit(value))

TEST("packing and then unpacking floats returns the original value") {
  SECTION("finite values compare equal") {
    CHECK_RT(0.f);
    CHECK_RT(0xCAFp1);
    CHECK_RT(flimits::epsilon());
    CHECK_RT(flimits::min());
    CHECK_RT(flimits::max());
    CHECK_RT(-0.f);
    CHECK_RT(0xCAFp1);
    CHECK_RT(-flimits::epsilon());
    CHECK_RT(-flimits::min());
    CHECK_RT(-flimits::max());
  }
  SECTION("packing and unpacking preserves infinity and NaN") {
    CHECK_PRED_RT(std::isinf, flimits::infinity());
    CHECK_PRED_RT(std::isinf, -flimits::infinity());
    CHECK_PRED_RT(std::isnan, flimits::quiet_NaN());
  }
  SECTION("packing and unpacking preserves the sign bit") {
    CHECK_SIGN_RT(0.f);
    CHECK_SIGN_RT(0xCAFp1);
    CHECK_SIGN_RT(flimits::epsilon());
    CHECK_SIGN_RT(flimits::min());
    CHECK_SIGN_RT(flimits::max());
    CHECK_SIGN_RT(flimits::infinity());
    CHECK_SIGN_RT(-0.f);
    CHECK_SIGN_RT(-0xCAFp1);
    CHECK_SIGN_RT(-flimits::epsilon());
    CHECK_SIGN_RT(-flimits::min());
    CHECK_SIGN_RT(-flimits::max());
    CHECK_SIGN_RT(-flimits::infinity());
  }
}

TEST("packing and then unpacking doubles returns the original value") {
  SECTION("finite values compare equal") {
    CHECK_RT(0.);
    CHECK_RT(0xCAFp1);
    CHECK_RT(dlimits::epsilon());
    CHECK_RT(dlimits::min());
    CHECK_RT(dlimits::max());
    CHECK_RT(-0.);
    CHECK_RT(0xCAFp1);
    CHECK_RT(-dlimits::epsilon());
    CHECK_RT(-dlimits::min());
    CHECK_RT(-dlimits::max());
  }
  SECTION("packing and unpacking preserves infinity and NaN") {
    CHECK_PRED_RT(std::isinf, dlimits::infinity());
    CHECK_PRED_RT(std::isinf, -dlimits::infinity());
    CHECK_PRED_RT(std::isnan, dlimits::quiet_NaN());
  }
  SECTION("packing and unpacking preserves the sign bit") {
    CHECK_SIGN_RT(0.);
    CHECK_SIGN_RT(0xCAFp1);
    CHECK_SIGN_RT(dlimits::epsilon());
    CHECK_SIGN_RT(dlimits::min());
    CHECK_SIGN_RT(dlimits::max());
    CHECK_SIGN_RT(dlimits::infinity());
    CHECK_SIGN_RT(-0.);
    CHECK_SIGN_RT(-0xCAFp1);
    CHECK_SIGN_RT(-dlimits::epsilon());
    CHECK_SIGN_RT(-dlimits::min());
    CHECK_SIGN_RT(-dlimits::max());
    CHECK_SIGN_RT(-dlimits::infinity());
  }
}

} // namespace
