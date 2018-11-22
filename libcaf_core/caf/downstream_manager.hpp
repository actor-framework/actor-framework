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

#include <memory>
#include <vector>

#include "caf/fwd.hpp"
#include "caf/stream_slot.hpp"

namespace caf {

/// Manages downstream communication for a `stream_manager`. The downstream
/// manager owns the `outbound_path` objects, has a buffer for storing pending
/// output and is responsible for the dispatching policy (broadcasting, for
/// example). The default implementation terminates the stream and never
/// accepts any pahts.
class downstream_manager {
public:
  // -- member types -----------------------------------------------------------

  /// Outbound path.
  using path_type = outbound_path;

  /// Pointer to an outbound path.
  using path_ptr = path_type*;

  /// Pointer to an immutable outbound path.
  using const_path_ptr = const path_type*;

  /// Unique pointer to an outbound path.
  using unique_path_ptr = std::unique_ptr<path_type>;

  /// Function object for iterating over all paths.
  struct path_visitor {
    virtual ~path_visitor();
    virtual void operator()(outbound_path& x) = 0;
  };

  /// Predicate object for paths.
  struct path_predicate {
    virtual ~path_predicate();
    virtual bool operator()(const outbound_path& x) const noexcept = 0;
  };

  /// Selects a check algorithms.
  enum path_algorithm {
    all_of,
    any_of,
    none_of
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit downstream_manager(stream_manager* parent);

  virtual ~downstream_manager();

  // -- properties -------------------------------------------------------------

  scheduled_actor* self() const noexcept;

  stream_manager* parent() const noexcept;

  /// Returns `true` if this manager belongs to a sink, i.e., terminates the
  /// stream and never has outbound paths.
  virtual bool terminal() const noexcept;

  // -- path management --------------------------------------------------------

  /// Applies `f` to each path.
  template <class F>
  void for_each_path(F f) {
    struct impl : path_visitor {
      F fun;
      impl(F x) : fun(std::move(x)) {
        // nop
      }
      void operator()(outbound_path& x) override {
        fun(x);
      }
    };
    impl g{std::move(f)};
    for_each_path_impl(g);
  }

  /// Returns all used slots.
  std::vector<stream_slot> path_slots();

  /// Returns all open slots, i.e., slots assigned to outbound paths with
  /// `closing == false`.
  std::vector<stream_slot> open_path_slots();

  /// Checks whether `predicate` holds true for all paths.
  template <class Predicate>
  bool all_paths(Predicate predicate) const noexcept {
    return check_paths(path_algorithm::all_of, std::move(predicate));
  }

  /// Checks whether `predicate` holds true for any path.
  template <class Predicate>
  bool any_path(Predicate predicate) const noexcept {
    return check_paths(path_algorithm::any_of, std::move(predicate));
  }

  /// Checks whether `predicate` holds true for no path.
  template <class Predicate>
  bool no_path(Predicate predicate) const noexcept {
    return check_paths(path_algorithm::none_of, std::move(predicate));
  }

  /// Returns the current number of paths.
  virtual size_t num_paths() const noexcept;

  /// Adds a pending path to `target` to the manager.
  /// @returns The added path on success, `nullptr` otherwise.
  path_ptr add_path(stream_slot slot, strong_actor_ptr target);

  /// Removes a path from the manager.
  virtual bool remove_path(stream_slot slot, error reason,
                           bool silent) noexcept;

  /// Returns the path associated to `slot` or `nullptr`.
  virtual path_ptr path(stream_slot slot) noexcept;

  /// Returns the path associated to `slot` or `nullptr`.
  const_path_ptr path(stream_slot slot) const noexcept;

  /// Returns `true` if there is no data pending and all batches are
  /// acknowledged batch on all paths.
  bool clean() const noexcept;

  /// Returns `true` if `slot` is unknown or if there is no data pending and
  /// all batches are acknowledged on `slot`. The default implementation
  /// returns `false` for all paths, even if `clean()` return `true`.
  bool clean(stream_slot slot) const noexcept;

  /// Removes all paths gracefully.
  virtual void close();

  /// Removes path `slot` gracefully by sending pending batches before removing
  /// it. Effectively calls `path(slot)->closing = true`.
  virtual void close(stream_slot slot);

  /// Removes all paths with an error message.
  virtual void abort(error reason);

  /// Returns `num_paths() == 0`.
  inline bool empty() const noexcept {
    return num_paths() == 0;
  }

  /// Returns the minimum amount of credit on all output paths.
  size_t min_credit() const;

  /// Returns the maximum amount of credit on all output paths.
  size_t max_credit() const;

  /// Returns the total amount of credit on all output paths, i.e., the sum of
  /// all individual credits.
  size_t total_credit() const;

  /// Sends batches to sinks.
  virtual void emit_batches();

  /// Sends batches to sinks regardless of whether or not the batches reach the
  /// desired batch size.
  virtual void force_emit_batches();

  /// Queries the currently available capacity for the output buffer.
  virtual size_t capacity() const noexcept;

  /// Queries the size of the output buffer.
  virtual size_t buffered() const noexcept;

  /// Queries an estimate of the size of the output buffer for `slot`.
  virtual size_t buffered(stream_slot slot) const noexcept;

  /// Computes the maximum available downstream capacity.
  virtual int32_t max_capacity() const noexcept;

  /// Queries whether the manager cannot make any progress, because its buffer
  /// is full and no more credit is available.
  bool stalled() const noexcept;

  /// Silently removes all paths.
  virtual void clear_paths();

protected:
  // -- customization points ---------------------------------------------------

  /// Inserts `ptr` to the implementation-specific container.
  virtual bool insert_path(unique_path_ptr ptr);

  /// Applies `f` to each path.
  virtual void for_each_path_impl(path_visitor& f);

  /// Dispatches the predicate to `std::all_of`, `std::any_of`, or
  /// `std::none_of`.
  virtual bool check_paths_impl(path_algorithm algo,
                                path_predicate& pred) const noexcept;

  /// Emits a regular (`reason == nullptr`) or irregular (`reason != nullptr`)
  /// shutdown if `silent == false`.
  /// @warning moves `*reason` if `reason == nullptr`
  virtual void about_to_erase(path_ptr ptr, bool silent, error* reason);

  // -- helper functions -------------------------------------------------------

  /// Delegates to `check_paths_impl`.
  template <class Predicate>
  bool check_paths(path_algorithm algorithm,
                   Predicate predicate) const noexcept {
    struct impl : path_predicate {
      Predicate fun;
      impl(Predicate x) : fun(std::move(x)) {
        // nop
      }
      bool operator()(const outbound_path& x) const noexcept override {
        return fun(x);
      }
    };
    impl g{std::move(predicate)};
    return check_paths_impl(algorithm, g);
  }

  // -- member variables -------------------------------------------------------

  stream_manager* parent_;
};

} // namespace caf

