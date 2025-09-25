#pragma once

#include "caf/cuda/global.hpp"
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <cuda.h>
#include <stdexcept>

namespace caf::cuda {

/// Thread-safe pool of CUDA streams.
/// 
/// Design goals:
///  - Limit total streams to `max_size` to avoid unbounded growth.
///  - Make the common case (reusing an available stream) as fast as possible:
///      a single short mutex-protected pop from `available_streams_`.
///  - When the pool is exhausted, safely reuse previously-created streams
///    (round-robin) from `all_streams_` rather than throwing.
///  - Track simple usage metrics (`total_streams` and `in_use`) for monitoring.
class CAF_CUDA_EXPORT StreamPool {
public:
  /// Construct pool with a given CUDA context.
  /// @param ctx CUDA context used to create streams.
  /// @param initial_size number of streams to pre-create (default 32).
  /// @param max_size maximum number of streams the pool may create (default 500).
  explicit StreamPool(CUcontext ctx,
                      size_t initial_size = 32,
                      size_t max_size = 500);

  ~StreamPool();

  /// Acquire a stream.
  /// Fast path: returns an already-available stream if one exists.
  /// If none are available and we are under `max_size`, creates a new stream.
  /// If `max_size` is reached and none are free, reuses a stream from `all_streams_`
  /// using round-robin selection.
  CUstream acquire();

  /// Release a stream back to the pool (marks it available).
  void release(CUstream s);

  /// Return current number of streams created (<= max_size).
  size_t num_total() const;

  /// Return current number of streams checked out (in use).
  size_t num_in_use() const;

  /// Return number of streams currently available in the pool.
  size_t num_available() const;

private:
  /// Create a new CUDA stream in the context `ctx_`.
  CUstream create_stream();

  CUcontext ctx_;                          ///< CUDA context used for stream creation
  std::deque<CUstream> available_streams_; ///< Queue of streams that are free for reuse
  std::vector<CUstream> all_streams_;      ///< All streams created by this pool (<= max_size_)
  size_t max_size_;                        ///< Maximum permitted streams
  size_t total_streams_ = 0;               ///< Number of streams created so far
  size_t in_use_ = 0;                      ///< Number of streams currently checked out
  size_t rr_index_ = 0;                    ///< Round-robin index into all_streams_ for reuse
  mutable std::mutex pool_mutex_;          ///< Protects the pool state
};

/// Per-device stream manager. Assigns streams to actor IDs.
/// 
/// `DeviceStreamTable` caches an assigned stream per `actor_id`. This makes the
/// common case (getting a stream for an actor who already has one) cheap:
/// a shared lock and hashmap lookup.
class CAF_CUDA_EXPORT DeviceStreamTable {
public:
  explicit DeviceStreamTable(CUcontext ctx, size_t pool_size = 32);

  /// Get the stream for an actor. If the actor has no assigned stream,
  /// allocate one from the pool and assign it.
  CUstream get_stream(int actor_id);

  /// Release the stream assigned to an actor back to the pool and erase the mapping.
  void release_stream(int actor_id);

private:
  StreamPool pool_;
  std::unordered_map<int, CUstream> table_; ///< actor_id -> stream
  mutable std::shared_mutex table_mutex_;   ///< Allows concurrent reads of table_
};

} // namespace caf::cuda

