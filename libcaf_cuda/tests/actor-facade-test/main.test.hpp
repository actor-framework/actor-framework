#pragma once

#include <caf/all.hpp>  // Includes most CAF essentials

#include "caf/cuda/actor_facade.hpp"
#include "caf/cuda/manager.hpp"
#include "caf/cuda/nd_range.hpp"
#include "caf/cuda/all.hpp"
#include <caf/type_id.hpp>
#include "caf/detail/test.hpp"
#include <cassert>

//function signatures of tests
void test_mmul(caf::actor_system& sys);
