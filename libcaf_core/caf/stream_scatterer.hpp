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

#ifndef CAF_STREAM_SCATTERER_HPP
#define CAF_STREAM_SCATTERER_HPP

#include <memory>

#include "caf/fwd.hpp"

namespace caf {

/// Type-erased policy for dispatching data to sinks.
class stream_scatterer {
public:
  // -- member types -----------------------------------------------------------

  /// Outbound path.
  using path_type = outbound_path;

  /// Pointer to an outbound path.
  using path_ptr = path_type*;

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

  explicit stream_scatterer(local_actor* self);

  virtual ~stream_scatterer();

  // -- properties -------------------------------------------------------------

  local_actor* self() const {
    return self_;
  }

  // -- meta information -------------------------------------------------------

  /// Returns `true` if thie scatterer belongs to a sink, i.e., terminates the
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
  virtual size_t num_paths() const noexcept = 0;

  /// Adds a path to `target` to the scatterer.
  /// @returns The added path on success, `nullptr` otherwise.
  virtual path_ptr add_path(stream_slots slots, strong_actor_ptr target) = 0;

  /// Removes a path from the scatterer and returns it.
  virtual unique_path_ptr take_path(stream_slot slots) noexcept = 0;

  /// Returns the path associated to `slots` or `nullptr`.
  virtual path_ptr path(stream_slot slots) noexcept = 0;

  /// Returns `true` if there is no data pending and no unacknowledged batch on
  /// any path.
  bool clean() const noexcept;

  /// Removes all paths gracefully.
  virtual void close();

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
  virtual void emit_batches() = 0;

  /// Sends batches to sinks regardless of whether or not the batches reach the
  /// desired batch size.
  virtual void force_emit_batches() = 0;

  /// Returns the currently available capacity for the output buffer.
  virtual size_t capacity() const noexcept = 0;

  /// Returns the size of the output buffer.
  virtual size_t buffered() const noexcept = 0;

  /// Returns `make_message(stream<T>{slot})`, where `T` is the value type of
  /// this scatterer.
  virtual message make_handshake_token(stream_slot slot) const = 0;

  /// Silently removes all paths.
  virtual void clear_paths() = 0;

protected:
  // -- customization points ---------------------------------------------------

  /// Applies `f` to each path.
  virtual void for_each_path_impl(path_visitor& f) = 0;

  /// Dispatches the predicate to `std::all_of`, `std::any_of`, or
  /// `std::none_of`.
  virtual bool check_paths_impl(path_algorithm algo,
                                path_predicate& pred) const noexcept = 0;

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

  local_actor* self_;
};

} // namespace caf

#endif // CAF_STREAM_SCATTERER_HPP
