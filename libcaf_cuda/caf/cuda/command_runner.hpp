#pragma once

#include "caf/cuda/command.hpp"
#include "caf/cuda/program.hpp"
#include "caf/cuda/nd_range.hpp"
#include "caf/cuda/platform.hpp"

namespace caf::cuda {

// ===========================================================================
// COMMAND RUNNER
// An Alternative gateway to the gpu, enabling users to create their own custom
// gpu actors if they wish 
// Manages synchronous and asynchronous command execution with overloads
// for actor_id, shared_memory, and device_number.
// ===========================================================================
template <class... Ts>
class command_runner {
public:
  using command_t = command<caf::actor, Ts...>;
  using base_command_t = base_command<caf::actor, Ts...>;

  // -------------------------------
  // Synchronous run: actor_id only
  // returns an output_buffer
  // -------------------------------
  template <class... Us>
  auto run(program_ptr program,
           nd_range dims,
           int actor_id,
           Us&&... xs) 
  {
      auto cmd = caf::make_counted<command_t>(std::move(program),
                                              std::move(dims),
                                              actor_id,
                                              std::forward<Us>(xs)...);
      return cmd->enqueue();
  }

  // -------------------------------
  // Synchronous run: actor_id + shared_memory
  // -------------------------------
  template <class... Us>
  auto run(program_ptr program,
           nd_range dims,
           int actor_id,
           int shared_memory,
           Us&&... xs) 
  {
      auto cmd = caf::make_counted<command_t>(std::move(program),
                                              std::move(dims),
                                              actor_id,
                                              shared_memory,
                                              std::forward<Us>(xs)...);
      return cmd->enqueue();
  }

  // -------------------------------
  // Synchronous run: actor_id + shared_memory + device_number
  // -------------------------------
  template <class... Us>
  auto run(program_ptr program,
           nd_range dims,
           int actor_id,
           int shared_memory,
           int device_number,
           Us&&... xs) 
  {
      auto cmd = caf::make_counted<command_t>(std::move(program),
                                              std::move(dims),
                                              actor_id,
                                              shared_memory,
                                              device_number,
                                              std::forward<Us>(xs)...);
      return cmd->enqueue();
  }

  // -------------------------------
  // Asynchronous run: actor_id only
  // returns a tuple of mem_ptrs
  // -------------------------------
  template <class... Us>
  auto run_async(program_ptr program,
                 nd_range dims,
                 int actor_id,
                 Us&&... xs) 
  {
      auto cmd = caf::make_counted<base_command_t>(std::move(program),
                                                   std::move(dims),
                                                   actor_id,
                                                   std::forward<Us>(xs)...);
      return cmd->base_enqueue();
  }

  // -------------------------------
  // Asynchronous run: actor_id + shared_memory
  // -------------------------------
  template <class... Us>
  auto run_async(program_ptr program,
                 nd_range dims,
                 int actor_id,
                 int shared_memory,
                 Us&&... xs) 
  {
      auto cmd = caf::make_counted<base_command_t>(std::move(program),
                                                   std::move(dims),
                                                   actor_id,
                                                   shared_memory,
                                                   std::forward<Us>(xs)...);
      return cmd->base_enqueue();
  }

  // -------------------------------
  // Asynchronous run: actor_id + shared_memory + device_number
  // -------------------------------
  template <class... Us>
  auto run_async(program_ptr program,
                 nd_range dims,
                 int actor_id,
                 int shared_memory,
                 int device_number,
                 Us&&... xs) 
  {
      auto cmd = caf::make_counted<base_command_t>(std::move(program),
                                                   std::move(dims),
                                                   actor_id,
                                                   shared_memory,
                                                   device_number,
                                                   std::forward<Us>(xs)...);
      return cmd->base_enqueue();
  }

  // -------------------------------
  // Destroy streams for a given actor ID
  // -------------------------------
  void release_stream_for_actor(int actor_id) {
      auto plat = platform::create();
      plat->release_streams_for_actor(actor_id);
  }
};

} // namespace caf::cuda

