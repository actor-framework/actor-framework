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

constexpr auto max_batch_delay = timespan{1'000'000};

/// Configures an algorithm for assigning credit and adjusting batch sizes.
///
/// The `size-based` controller (default) samples how many Bytes stream elements
/// occupy when serialized to CAF's binary wire format.
///
/// The `token-based` controller associates each stream element with one token.
/// Input buffer and batch sizes are then statically defined in terms of tokens.
/// This strategy makes no dynamic adjustment or sampling.
constexpr auto credit_policy = string_view{"size-based"};

[[deprecated("this parameter no longer has any effect")]] //
constexpr auto credit_round_interval
  = max_batch_delay;

} // namespace caf::defaults::stream

namespace caf::defaults::stream::size_policy {

/// Desired size of a single batch in Bytes, when serialized into CAF's binary
/// wire format.
constexpr auto bytes_per_batch = int32_t{2 * 1024}; //  2 KB

/// Number of Bytes (over all received elements) an inbound path may buffer.
/// Actors use heuristics for calculating the estimated memory use, so actors
/// may still allocate more memory in practice.
constexpr auto buffer_capacity = int32_t{64 * 1024}; // 64 KB

/// Frequency of computing the serialized size of incoming batches. Smaller
/// values may increase accuracy, but also add computational overhead.
constexpr auto sampling_rate = int32_t{25};

/// Frequency of re-calibrating batch sizes. For example, a calibration interval
/// of 10 and a sampling rate of 20 causes the actor to re-calibrate every 200
/// batches.
constexpr auto calibration_interval = int32_t{20};

/// Value between 0 and 1 representing the degree of weighting decrease for
/// adjusting batch sizes. A higher factor discounts older observations faster.
constexpr auto smoothing_factor = float{0.6};

} // namespace caf::defaults::stream::size_policy

namespace caf::defaults::stream::token_policy {

/// Number of elements in a single batch.
constexpr auto batch_size = int32_t{256}; // 2 KB for elements of size 8.

/// Maximum number of elements in the input buffer.
constexpr auto buffer_size = int32_t{4096}; // // 32 KB for elements of size 8.

} // namespace caf::defaults::stream::token_policy

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
