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

#ifndef CAF_BROKER_STATS_HPP
#define CAF_BROKER_STATS_HPP

#include "caf/instrumentation/instrumentation_ids.hpp"
#include "caf/instrumentation/stat_stream.hpp"

#include <unordered_map>
#include <cstdint>
#include <string>
#include <mutex>

namespace caf {
namespace instrumentation {

/// Instrumentation stats aggregated per-worker for all callsites.
class broker_stats {
  friend class lockable_broker_stats;

public:
  std::string to_string() const;

  const std::unordered_map<msgtype_id, stat_stream>& get_forward_wait_durations() const;
  const stat_stream& get_forward_size() const;
  const std::unordered_map<msgtype_id, size_t>& get_message_counts() const;

protected:
  std::unordered_map<msgtype_id, stat_stream> forward_waittimes_;
  stat_stream forward_mb_size_;
  std::unordered_map<msgtype_id, size_t> receive_msg_count_;
};

class lockable_broker_stats : public broker_stats {
public:
  void record_broker_receive(msgtype_id mt);
  void record_broker_forward(msgtype_id mt, int64_t mb_waittime, size_t mb_size);
  broker_stats collect();

private:
  std::mutex access_mutex_;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_BROKER_STATS_HPP
