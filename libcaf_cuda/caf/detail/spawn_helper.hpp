#pragma once

#include <functional>

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/cuda/actor_facade.hpp"
#include "caf/cuda/program.hpp"
#include "caf/cuda/nd_range.hpp"
#include <type_traits>

//file that aids manager to help spawn in the actor facade
namespace caf {
namespace detail {

template <bool PassConfig, class... Ts>
struct cuda_spawn_helper {
  using impl = cuda::actor_facade<PassConfig, std::decay_t<Ts>...>;
 
  //this operator should spawn in a facade with a program
  actor operator()(
		  actor_system * sys,
		  actor_config&& cfg,
		   caf::cuda::program_ptr prog,
		   caf::cuda::nd_range dims,
                   Ts&&... xs) const {
    return actor_cast<actor>(impl::create(
			    sys,
			    std::move(cfg),              
			    prog,
			    dims,
			    std::forward<Ts>(xs)...));
  }



};

} // namespace detail
} // namespace caf

