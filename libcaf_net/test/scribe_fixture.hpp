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

#pragma once

#include <stdexcept>

#include "caf/error.hpp"

namespace {

struct transport_mock {
  transport_mock() = default;
  ~transport_mock() = default;
};


struct application_mock {
  application_mock() = default;
  ~application_mock() = default;
};

template <class Application, class Transport>
struct parent_mock {
  using application_type = Application;
  using transport_type = Transport;

  parent_mock() = default;
  ~parent_mock() = default;

  application_type& application() {
    return application_;
  };

  transport_type& transport() {
    return transport_;
  }
private:
  transport_type transport_;
  application_type application_;
};

} // namespace
