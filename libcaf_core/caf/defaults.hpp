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
#include <string>
#include <vector>

#include "caf/config.hpp"
#include "caf/atom.hpp"
#include "caf/string_view.hpp"
#include "caf/timestamp.hpp"

// -- hard-coded default values for various CAF options ------------------------

namespace caf {
namespace defaults {

namespace stream {

extern CAF_API const timespan desired_batch_complexity;
extern CAF_API const timespan max_batch_delay;
extern CAF_API const timespan credit_round_interval;

} // namespace streaming

namespace scheduler {

extern CAF_API const atom_value policy;
extern CAF_API string_view profiling_output_file;
extern CAF_API const size_t max_threads;
extern CAF_API const size_t max_throughput;
extern CAF_API const timespan profiling_resolution;

} // namespace scheduler

namespace work_stealing {

extern CAF_API const size_t aggressive_poll_attempts;
extern CAF_API const size_t aggressive_steal_interval;
extern CAF_API const size_t moderate_poll_attempts;
extern CAF_API const size_t moderate_steal_interval;
extern CAF_API const timespan moderate_sleep_duration;
extern CAF_API const size_t relaxed_steal_interval;
extern CAF_API const timespan relaxed_sleep_duration;

} // namespace work_stealing

namespace logger {

extern CAF_API string_view component_filter;
extern CAF_API const atom_value console;
extern CAF_API string_view console_format;
extern CAF_API const atom_value console_verbosity;
extern CAF_API string_view file_format;
extern CAF_API string_view file_name;
extern CAF_API const atom_value file_verbosity;

} // namespace logger

namespace middleman {

extern CAF_API std::vector<std::string> app_identifiers;
extern CAF_API const atom_value network_backend;
extern CAF_API const size_t max_consecutive_reads;
extern CAF_API const size_t heartbeat_interval;
extern CAF_API const size_t cached_udp_buffers;
extern CAF_API const size_t max_pending_msgs;
extern CAF_API const size_t workers;

} // namespace middleman

} // namespace defaults
} // namespace caf
