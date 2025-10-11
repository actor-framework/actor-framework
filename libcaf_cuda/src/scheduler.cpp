#include "caf/cuda/scheduler.hpp"
#include <stdexcept>
#include <cstdlib> // for rand()
#include <iostream>

namespace caf::cuda {

// ---------------------- scheduler base class ----------------------

template <typename T>
mem_ptr<T> scheduler::transfer_mem_ref(const mem_ptr<T>& src,
                                       device_ptr target_device) {
  if (src->is_scalar()) {
    return mem_ptr<T>(new mem_ref<T>(src->host_scalar_ptr()[0], src->access(),
                                     target_device->getId(), 0, nullptr));
  }

  auto host_data = src->copy_to_host();
  size_t bytes = src->size() * sizeof(T);
  CUdeviceptr new_mem;

  CHECK_CUDA(cuCtxPushCurrent(target_device->getContext()));
  CHECK_CUDA(cuMemAlloc(&new_mem, bytes));
  CHECK_CUDA(cuMemcpyHtoD(new_mem, host_data.data(), bytes));
  CHECK_CUDA(cuCtxPopCurrent(nullptr));

  return mem_ptr<T>(new mem_ref<T>(src->size(), new_mem, src->access(),
                                   target_device->getId(), 0, nullptr));
}

// ---------------------- single_device_scheduler ----------------------

void single_device_scheduler::set_devices(const std::vector<device_ptr>& devices) {
  devices_ = devices;
}

device_ptr single_device_scheduler::schedule([[maybe_unused]] int actor_id) {
  if (devices_.empty()) {
    throw std::runtime_error("No devices available");
  }
  return devices_[0];
}

device_ptr single_device_scheduler::schedule([[maybe_unused]] int actor_id,
                                             [[maybe_unused]] int device_number) {
  if (devices_.empty()) {
    throw std::runtime_error("No devices available");
  }
  return devices_[0];
}

void single_device_scheduler::getStreamAndContext(int actor_id, CUcontext* context,
                                                  CUstream* stream) {
  auto dev = schedule(actor_id);
  *context = dev->getContext();
  *stream = dev->get_stream_for_actor(actor_id);
}

device_ptr single_device_scheduler::find_device_by_id(int id) {
  return (devices_.empty() || devices_[0]->getId() != id) ? nullptr : devices_[0];
}

// ---------------------- multi_device_scheduler ----------------------

void multi_device_scheduler::set_devices(const std::vector<device_ptr>& devices) {
  devices_ = devices;
}

device_ptr multi_device_scheduler::schedule([[maybe_unused]] int actor_id) {
  if (devices_.empty()) {
    throw std::runtime_error("No devices available");
  }
  size_t num_devices = devices_.size();
  size_t device_index = static_cast<size_t>(std::rand()) % num_devices;
  return devices_[device_index];
}

device_ptr multi_device_scheduler::schedule([[maybe_unused]] int actor_id,
                                            int device_number) {
  if (devices_.empty()) {
    throw std::runtime_error("No devices available");
  }
  size_t num_devices = devices_.size();
  size_t device_index = static_cast<size_t>(device_number) % num_devices;
  return devices_[device_index];
}

void multi_device_scheduler::getStreamAndContext(int actor_id, CUcontext* context,
                                                 CUstream* stream) {
  auto dev = schedule(actor_id);
  *context = dev->getContext();
  *stream = dev->get_stream_for_actor(actor_id);
}

device_ptr multi_device_scheduler::find_device_by_id(int id) {
  for (const auto& dev : devices_) {
    if (dev->getId() == id) {
      return dev;
    }
  }
  return nullptr;
}

} // namespace caf::cuda

