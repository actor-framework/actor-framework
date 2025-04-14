// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/build_config.hpp"
#include "caf/detail/log_level.hpp"
#include "caf/timestamp.hpp"

#include <chrono>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

// -- hard-coded default values for various CAF options ------------------------

namespace caf::defaults {

/// Stores the name of a parameter along with the fallback value.
template <class T>
struct parameter {
  std::string_view name;
  T fallback;
};

/// @relates parameter
template <class T>
constexpr parameter<T> make_parameter(std::string_view name, T fallback) {
  return {name, fallback};
}

/// Configures how many actions scheduled_actor::delay may add to the internal
/// queue for scheduled_actor::run_actions before being forced to push them to
/// the mailbox instead.
constexpr auto max_inline_actions_per_run = size_t{10};

} // namespace caf::defaults

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
constexpr auto credit_policy = std::string_view{"size-based"};

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
constexpr auto sampling_rate = int32_t{100};

/// Frequency of re-calibrating batch sizes. For example, a calibration interval
/// of 10 and a sampling rate of 20 causes the actor to re-calibrate every 200
/// batches.
constexpr auto calibration_interval = int32_t{20};

/// Value between 0 and 1 representing the degree of weighting decrease for
/// adjusting batch sizes. A higher factor discounts older observations faster.
constexpr auto smoothing_factor = 0.6f;

} // namespace caf::defaults::stream::size_policy

namespace caf::defaults::stream::token_policy {

/// Number of elements in a single batch.
constexpr auto batch_size = int32_t{256}; // 2 KB for elements of size 8.

/// Maximum number of elements in the input buffer.
constexpr auto buffer_size = int32_t{4096}; // // 32 KB for elements of size 8.

} // namespace caf::defaults::stream::token_policy

namespace caf::defaults::scheduler {

constexpr auto policy = std::string_view{"stealing"};
constexpr auto max_throughput = std::numeric_limits<size_t>::max();

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

constexpr auto format = std::string_view{"%r %c %p %a %t %M %F:%L %m%n"};
constexpr auto verbosity = std::string_view{"quiet"};
constexpr auto path
  = std::string_view{"actor_log_[PID]_[TIMESTAMP]_[NODE].log"};

} // namespace caf::defaults::logger::file

namespace caf::defaults::logger::console {

constexpr auto colored = true;
constexpr auto verbosity = std::string_view{"error"};
constexpr auto format = std::string_view{"[%c:%p] %d %m"};

} // namespace caf::defaults::logger::console

namespace caf::defaults::middleman {

constexpr auto app_identifier = std::string_view{"generic-caf-app"};
constexpr auto cached_udp_buffers = size_t{10};
constexpr auto connection_timeout = timespan{30'000'000'000};
constexpr auto heartbeat_interval = timespan{10'000'000'000};
constexpr auto max_consecutive_reads = size_t{50};
constexpr auto max_pending_msgs = size_t{10};
constexpr auto network_backend = std::string_view{"default"};

} // namespace caf::defaults::middleman

namespace caf::defaults::flow {

/// Defines how much demand should accumulate before signaling demand upstream.
/// A minimum demand is used by operators such as `observe_on` to avoid overly
/// frequent signaling across asynchronous barriers.
constexpr auto min_demand = size_t{8};

/// Defines how many items a single batch may contain.
constexpr auto batch_size = size_t{32};

/// Limits how many items an operator buffers internally.
constexpr auto buffer_size = size_t{128};

/// Limits the number of concurrent subscriptions for operators such as `merge`.
constexpr auto max_concurrent = size_t{8};

} // namespace caf::defaults::flow

namespace caf::defaults::net {

/// Configures how many concurrent connections an acceptor allows. When reaching
/// this limit, the connector stops accepting additional connections until a
/// previous connection has been closed.
constexpr auto max_connections = make_parameter("max-connections", size_t{64});

constexpr auto max_consecutive_reads = make_parameter("max-consecutive-reads",
                                                      size_t{50});

/// Default maximum size for incoming HTTP requests: 64KiB.
constexpr auto http_max_request_size = uint32_t{65'536};

/// The default port for HTTP servers.
constexpr auto http_default_port = uint16_t{80};

/// The default port for HTTPS servers.
constexpr auto https_default_port = uint16_t{443};

/// The default buffer size for reading and writing octet streams.
constexpr auto octet_stream_buffer_size = uint32_t{1024};

} // namespace caf::defaults::net
