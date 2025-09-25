#pragma once

#include <tuple>
#include <vector>
#include <iostream>

#include <caf/abstract_actor.hpp>
#include <caf/intrusive_ptr.hpp>
#include <caf/message.hpp>

#include "caf/cuda/global.hpp"
#include "caf/cuda/nd_range.hpp"
#include "caf/cuda/platform.hpp"
#include "caf/cuda/mem_ref.hpp"
#include "caf/cuda/device.hpp"



//These classes represent an abstraction of a single kernel launch
//they are not meant to be visbile by the programmer if you need to 
//launch a kernel use actor facade or the command runner class
namespace caf::cuda {

// ===========================================================================
// BASE COMMAND
// This class will always launch and schedule a kernel execution and return a
// tuple of mem_ptr's.
// This classes methods are asynchronous meaning that the memory in the 
// mem_ptrs may or may not still be getting worked on
// ===========================================================================
template <class Actor, class... Ts>
class base_command : public ref_counted {
public:
  // -------------------------------
  // Constructor: actor_id only
  // -------------------------------
  template <typename... Us>
  base_command(program_ptr program,
               nd_range dims,
               int actor_id,
               Us&&... xs)
      : program_(std::move(program)),
        dims_(std::move(dims)),
        actor_id(actor_id),
        kernel_args(std::make_tuple(std::forward<Us>(xs)...)),
        shared_memory_(0)
  {
      dev_ = platform::create()->schedule(actor_id);
      static_assert(sizeof...(Us) == sizeof...(Ts), "Argument count mismatch");
  }

  // -------------------------------
  // Constructor: actor_id + shared_memory
  // -------------------------------
  template <typename... Us>
  base_command(program_ptr program,
               nd_range dims,
               int actor_id,
               int shared_memory,
               Us&&... xs)
      : program_(std::move(program)),
        dims_(std::move(dims)),
        actor_id(actor_id),
        kernel_args(std::make_tuple(std::forward<Us>(xs)...)),
        shared_memory_(shared_memory)
  {
      dev_ = platform::create()->schedule(actor_id);
      static_assert(sizeof...(Us) == sizeof...(Ts), "Argument count mismatch");
  }

  // -------------------------------
  // Constructor: actor_id + shared_memory + device_number
  // -------------------------------
  template <typename... Us>
  base_command(program_ptr program,
               nd_range dims,
               int actor_id,
               int shared_memory,
               int device_number,
               Us&&... xs)
      : program_(std::move(program)),
        dims_(std::move(dims)),
        actor_id(actor_id),
        kernel_args(std::make_tuple(std::forward<Us>(xs)...)),
        shared_memory_(shared_memory)
  {
      if (device_number == -1)
          dev_ = platform::create()->schedule(actor_id);
      else
          dev_ = platform::create()->schedule(actor_id, device_number);

      static_assert(sizeof...(Us) == sizeof...(Ts), "Argument count mismatch");
  }

  virtual ~base_command() = default;

  // -------------------------------------------------------------------------
  // Unpacks a caf message and calls launch_kernel_mem_ref
  // Returns tuple of mem_ptrs/handles of memory on the gpu
  // -------------------------------------------------------------------------
  virtual std::tuple<mem_ptr<raw_t<Ts>>...> base_enqueue() {
      CUfunction kernel = program_->get_kernel(dev_->getId());
      return dev_->launch_kernel_mem_ref(kernel, dims_, kernel_args, actor_id, shared_memory_);
  }

protected:
  program_ptr program_;
  nd_range dims_;
  int actor_id;
  device_ptr dev_;
  std::tuple<Ts ...> kernel_args;
  int shared_memory_;
};

// ===========================================================================
// COMMAND
// This class returns an output buffer instead of mem_ptr tuple and handles
// mem_ref cleanup.
// this classes calls are synchronious, meaning that the memory in the buffers
// are guarenteed to be there
// ===========================================================================
template <class Actor, class... Ts>
class command : public base_command<Actor, Ts...> {
public:
  using base = base_command<Actor, Ts...>;

  template <typename... Us>
  command(program_ptr program,
          nd_range dims,
          int actor_id,
          Us&&... xs)
      : base(std::move(program), std::move(dims), actor_id, std::forward<Us>(xs)...) {}

  template <typename... Us>
  command(program_ptr program,
          nd_range dims,
          int actor_id,
          int shared_memory,
          Us&&... xs)
      : base(std::move(program), std::move(dims), actor_id, shared_memory, std::forward<Us>(xs)...) {}

  template <typename... Us>
  command(program_ptr program,
          nd_range dims,
          int actor_id,
          int shared_memory,
          int device_number,
          Us&&... xs)
      : base(std::move(program), std::move(dims), actor_id, shared_memory, device_number, std::forward<Us>(xs)...) {}

  // -------------------------------------------------------------------------
  // Overrides base_enqueue to return collected output_buffers
  // -------------------------------------------------------------------------
  std::vector<output_buffer> enqueue() {
      auto mem_refs = base::base_enqueue();
      return base::dev_->collect_output_buffers(mem_refs);
  }
};

} // namespace caf::cuda

