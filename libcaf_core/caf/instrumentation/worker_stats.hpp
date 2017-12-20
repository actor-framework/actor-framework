/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_WORKER_STATS_HPP
#define CAF_WORKER_STATS_HPP

#include "caf/instrumentation/instrumentation_ids.hpp"
#include "caf/instrumentation/stat_stream.hpp"

#include <unordered_map>
#include <utility>
#include <cstdint>
#include <string>
#include <mutex>

namespace caf {
namespace instrumentation {
/// Instrumentation stats aggregated per-worker for all callsites.
class worker_stats {
  friend class lockable_worker_stats;
  using  typed_individual = std::unordered_map<sender, stat_stream>;
  using  individual       = std::unordered_map<instrumented_actor_id, stat_stream>;
  using  typed_aggregate  = std::unordered_map<aggregate_sender, stat_stream>;
  using  aggregate        = std::unordered_map<actortype_id, stat_stream>;
  using  individual_count = std::unordered_map<sender, size_t>;
  using  aggregate_count  = std::unordered_map<aggregate_sender, size_t>;

public:
  void combine(const worker_stats& rhs);
  std::string to_string() const;

  const typed_individual& get_individual_behavior_wait_durations() const;
  const typed_aggregate&  get_aggregated_behavior_wait_durations() const;
  const individual& get_individual_mailbox_sizes() const;
  const aggregate&  get_aggregated_mailbox_sizes() const;
  const typed_individual& get_individual_request_durations() const;
  const typed_aggregate&  get_aggregate_request_durations() const;
  const individual_count& get_individual_send_count() const;
  const aggregate_count&  get_aggregate_send_count() const;


protected:
  typed_individual behavior_individual_waittime_;
  typed_aggregate  behavior_aggregate_waittime_;
  individual behavior_individual_mbsize_;
  aggregate  behavior_aggregate_mbsize_;
  typed_individual request_individual_times_;
  typed_aggregate request_aggregate_times_;
  individual_count send_individual_count_;
  aggregate_count  send_aggregate_count_;

};

class lockable_worker_stats : public worker_stats {
public:
  void record_behavior_individual(instrumented_actor_id aid, msgtype_id mt, int64_t mb_waittime, size_t mb_size);
  void record_behavior_aggregate(actortype_id at, msgtype_id mt, int64_t mb_waittime, size_t mb_size);
  void record_request_individual(instrumented_actor_id aid, msgtype_id mt, int64_t waittime);
  void record_request_aggregate(actortype_id at, msgtype_id mt, int64_t waittime);
  void record_send_individual(instrumented_actor_id aid, msgtype_id mt);
  void record_send_aggregate(actortype_id at, msgtype_id mt);
  worker_stats collect();

private:
  std::mutex access_mutex_;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_WORKER_STATS_HPP
