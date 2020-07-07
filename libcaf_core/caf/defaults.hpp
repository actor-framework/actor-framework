/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <chrono>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "caf/detail/build_config.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/string_view.hpp"
#include "caf/timestamp.hpp"

// -- hard-coded default values for various CAF options ------------------------

namespace caf::defaults::stream {

constexpr auto desired_batch_complexity = timespan{50'000};
constexpr auto max_batch_delay = timespan{5'000'000};
constexpr auto credit_round_interval = timespan{10'000'000};
constexpr auto credit_policy = string_view{"complexity"};

} // namespace caf::defaults::stream

namespace caf::defaults::stream::size_policy {

constexpr auto bytes_per_batch = int32_t{02 * 1024}; //  2 KB
constexpr auto buffer_capacity = int32_t{64 * 1024}; // 64 KB

} // namespace caf::defaults::stream::size_policy

namespace caf::defaults::scheduler {

constexpr auto policy = string_view{"stealing"};
constexpr auto profiling_output_file = string_view{""};
constexpr auto max_throughput = std::numeric_limits<size_t>::max();
constexpr auto profiling_resolution = timespan(100'000'000);

} // namespace caf::defaults::scheduler

namespace caf::defaults::work_stealing {

constexpr auto aggressive_poll_attempts = size_t{100};
constexpr auto aggressive_steal_interval = size_t{10};
constexpr auto moderate_poll_attempts = size_t{500};
constexpr auto moderate_steal_interval = size_t{5};
constexpr auto moderate_sleep_duration = timespan{50'000};
constexpr auto relaxed_steal_interval = size_t{1};
constexpr auto relaxed_sleep_duration = timespan{10'000'000};

} // namespace caf::defaults::work_stealing

namespace caf::defaults::logger::file {

constexpr auto format = string_view{"%r %c %p %a %t %C %M %F:%L %m%n"};
constexpr auto path = string_view{"actor_log_[PID]_[TIMESTAMP]_[NODE].log"};

} // namespace caf::defaults::logger::file

namespace caf::defaults::logger::console {

constexpr auto colored = true;
constexpr auto format = string_view{"[%c:%p] %d %m"};

} // namespace caf::defaults::logger::console

namespace caf::defaults::middleman {

constexpr auto app_identifier = string_view{"generic-caf-app"};
constexpr auto network_backend = string_view{"default"};
constexpr auto max_consecutive_reads = size_t{50};
constexpr auto heartbeat_interval = size_t{0};
constexpr auto cached_udp_buffers = size_t{10};
constexpr auto max_pending_msgs = size_t{10};

} // namespace caf::defaults::middleman
