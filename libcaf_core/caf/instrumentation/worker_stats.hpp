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

public:
  void combine(const worker_stats& rhs);
  std::string to_string() const;

protected:
  std::unordered_map<std::pair<actortype_id, msgtype_id>, stat_stream> msg_waittimes_;
  std::unordered_map<actortype_id, stat_stream> mb_sizes_;
  std::unordered_map<std::pair<actortype_id, msgtype_id>, stat_stream> request_times_;
};

class lockable_worker_stats : public worker_stats {
public:
  void record_pre_behavior(actortype_id at, msgtype_id mt, int64_t mb_waittime, size_t mb_size);
  void record_request(actortype_id at, msgtype_id mt, int64_t waittime);
  worker_stats collect();

private:
  std::mutex access_mutex_;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_WORKER_STATS_HPP
