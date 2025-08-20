#pragma once

#include <caf/all.hpp>
#include <caf/cuda/manager.hpp>
#include <caf/cuda/program.hpp>
#include <caf/cuda/device.hpp>
#include <caf/cuda/command.hpp>
#include <caf/cuda/actor_facade.hpp>
#include <caf/cuda/mem_ref.hpp>
#include <caf/cuda/platform.hpp>
#include <caf/cuda/global.hpp>

// Test function declarations
void test_platform(caf::actor_system& sys);
void test_device(caf::actor_system& sys);
void test_manager(caf::actor_system& sys);
void test_program(caf::actor_system& sys);
void test_mem_ref(caf::actor_system& sys);
void test_command(caf::actor_system& sys);
void test_actor_facade(caf::actor_system& sys);
void CAF_CUDA_EXPORT test_main(caf::actor_system& sys);
