/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE error

#include "caf/error.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

CAF_TEST(default constructed errors evaluate to false) {
  error err;
  CAF_CHECK(!err);
}

CAF_TEST(error code zero is not an error) {
  CAF_CHECK(!error(0, error_category<sec>::value));
  CAF_CHECK(!make_error(sec::none));
  CAF_CHECK(!error{error_code<sec>(sec::none)});
}

CAF_TEST(error codes that are not zero are errors) {
  CAF_CHECK(error(1, error_category<sec>::value));
  CAF_CHECK(make_error(sec::unexpected_message));
  CAF_CHECK(error{error_code<sec>(sec::unexpected_message)});
}

CAF_TEST(errors convert enums to their integer value) {
  CAF_CHECK_EQUAL(make_error(sec::unexpected_message).code(), 1u);
  CAF_CHECK_EQUAL(error{error_code<sec>(sec::unexpected_message)}.code(), 1u);
}
