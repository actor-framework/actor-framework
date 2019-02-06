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

#include "caf/defaults.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>

namespace {

using us_t = std::chrono::microseconds;

constexpr caf::timespan us(us_t::rep x) {
  return std::chrono::duration_cast<caf::timespan>(us_t{x});
}

using ms_t = std::chrono::milliseconds;

constexpr caf::timespan ms(ms_t::rep x) {
  return std::chrono::duration_cast<caf::timespan>(ms_t{x});
}

} // namespace <anonymous>

namespace caf {
namespace defaults {

namespace stream {

const timespan desired_batch_complexity = us(50);
const timespan max_batch_delay = ms(5);
const timespan credit_round_interval = ms(10);

} // namespace stream

namespace scheduler {

const atom_value policy = atom("stealing");
string_view profiling_output_file = "";
const size_t max_threads = std::max(std::thread::hardware_concurrency(), 4u);
const size_t max_throughput = std::numeric_limits<size_t>::max();
const timespan profiling_resolution = ms(100);

} // namespace scheduler

namespace work_stealing {

const size_t aggressive_poll_attempts = 100;
const size_t aggressive_steal_interval = 10;
const size_t moderate_poll_attempts = 500;
const size_t moderate_steal_interval = 5;
const timespan moderate_sleep_duration = us(50);
const size_t relaxed_steal_interval = 1;
const timespan relaxed_sleep_duration = ms(10);

} // namespace work_stealing

namespace logger {

string_view component_filter = "";
const atom_value console = atom("none");
string_view console_format = "%m";
const atom_value console_verbosity = atom("trace");
string_view file_format = "%r %c %p %a %t %C %M %F:%L %m%n";
string_view file_name = "actor_log_[PID]_[TIMESTAMP]_[NODE].log";
const atom_value file_verbosity = atom("trace");

} // namespace logger

namespace middleman {

std::vector<std::string> app_identifiers{"generic-caf-app"};
const atom_value network_backend = atom("default");
const size_t max_consecutive_reads = 50;
const size_t heartbeat_interval = 0;
const size_t cached_udp_buffers = 10;
const size_t max_pending_msgs = 10;

} // namespace middleman

} // namespace defaults
} // namespace caf
