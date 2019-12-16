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

#include "caf/detail/core_export.hpp"
#include "caf/string_view.hpp"
#include "caf/timestamp.hpp"

// -- hard-coded default values for various CAF options ------------------------

namespace caf::defaults {

namespace stream {

extern CAF_CORE_EXPORT const timespan desired_batch_complexity;
extern CAF_CORE_EXPORT const timespan max_batch_delay;
extern CAF_CORE_EXPORT const timespan credit_round_interval;
extern CAF_CORE_EXPORT const string_view credit_policy;

namespace size_policy {

extern CAF_CORE_EXPORT const int32_t bytes_per_batch;
extern CAF_CORE_EXPORT const int32_t buffer_capacity;

} // namespace size_policy

} // namespace stream

namespace scheduler {

extern CAF_CORE_EXPORT const string_view policy;
extern CAF_CORE_EXPORT string_view profiling_output_file;
extern CAF_CORE_EXPORT const size_t max_threads;
extern CAF_CORE_EXPORT const size_t max_throughput;
extern CAF_CORE_EXPORT const timespan profiling_resolution;

} // namespace scheduler

namespace work_stealing {

extern CAF_CORE_EXPORT const size_t aggressive_poll_attempts;
extern CAF_CORE_EXPORT const size_t aggressive_steal_interval;
extern CAF_CORE_EXPORT const size_t moderate_poll_attempts;
extern CAF_CORE_EXPORT const size_t moderate_steal_interval;
extern CAF_CORE_EXPORT const timespan moderate_sleep_duration;
extern CAF_CORE_EXPORT const size_t relaxed_steal_interval;
extern CAF_CORE_EXPORT const timespan relaxed_sleep_duration;

} // namespace work_stealing

namespace logger {

extern CAF_CORE_EXPORT string_view component_filter;
extern CAF_CORE_EXPORT const string_view console;
extern CAF_CORE_EXPORT string_view console_format;
extern CAF_CORE_EXPORT const string_view console_verbosity;
extern CAF_CORE_EXPORT string_view file_format;
extern CAF_CORE_EXPORT string_view file_name;
extern CAF_CORE_EXPORT const string_view file_verbosity;

} // namespace logger

namespace middleman {

extern CAF_CORE_EXPORT std::vector<std::string> app_identifiers;
extern CAF_CORE_EXPORT const string_view network_backend;
extern CAF_CORE_EXPORT const size_t max_consecutive_reads;
extern CAF_CORE_EXPORT const size_t heartbeat_interval;
extern CAF_CORE_EXPORT const size_t cached_udp_buffers;
extern CAF_CORE_EXPORT const size_t max_pending_msgs;
extern CAF_CORE_EXPORT const size_t workers;

} // namespace middleman

} // namespace caf::defaults
