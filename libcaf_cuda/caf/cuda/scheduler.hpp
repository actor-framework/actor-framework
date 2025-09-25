#pragma once

#include <vector>
#include <tuple>
#include "caf/cuda/global.hpp"
#include "caf/cuda/device.hpp"

namespace caf::cuda {

class device; // Forward declaration
using device_ptr = caf::intrusive_ptr<device>;

/// Base class for GPU scheduling strategies.
/// Determines which device a kernel should run on
/// and handles argument conversion if necessary.
class CAF_CUDA_EXPORT scheduler {
public:
  virtual ~scheduler() = default;

  /// Returns the device an actor should run on
  virtual device_ptr schedule([[maybe_unused]] int actor_id) = 0;
  virtual device_ptr schedule([[maybe_unused]] int actor_id,
                              [[maybe_unused]] int device_number) = 0;

  /// Assigns the context and stream for the scheduled device
  virtual void getStreamAndContext(int actor_id, CUcontext* context,
                                   CUstream* stream) = 0;

  /// Checks arguments for mem_refs and returns the device of the first mem_ref found
  template <typename... Ts>
  device_ptr checkArgs(const Ts&... args) {
    return checkArgsImpl(std::make_tuple(args...));
  }

  /// Converts mem_refs to the target device if necessary
  template <typename... Ts>
  void convertArgs(device_ptr target_device, Ts&... args) {
    convertArgsImpl(target_device, std::make_tuple(std::ref(args)...));
  }

  /// Sets the list of devices
  virtual void set_devices(const std::vector<device_ptr>& devices) = 0;

protected:
  /// Base case for argument checking recursion
  virtual device_ptr checkArgsImpl(const std::tuple<>&) {
    return nullptr;
  }

  /// Base case for argument conversion recursion
  virtual void convertArgsImpl(device_ptr, std::tuple<>&) {}

private:
  /// Helper to identify mem_ptr types
  template <typename T>
  struct is_mem_ptr : std::false_type {};

  template <typename T>
  struct is_mem_ptr<mem_ptr<T>> : std::true_type {};

  /// Finds a device by its ID
  virtual device_ptr find_device_by_id(int id) = 0;

  /// Transfers a mem_ref to a target device
  template <typename T>
  mem_ptr<T> transfer_mem_ref(const mem_ptr<T>& src, device_ptr target_device);
};

/// Scheduler for a single device. Always chooses the same device.
class single_device_scheduler : public scheduler {
public:
  void set_devices(const std::vector<device_ptr>& devices) override;
  device_ptr schedule([[maybe_unused]] int actor_id) override;
  device_ptr schedule([[maybe_unused]] int actor_id,
                      [[maybe_unused]] int device_number) override;
  void getStreamAndContext(int actor_id, CUcontext* context,
                           CUstream* stream) override;

private:
  device_ptr find_device_by_id(int id) override;

  std::vector<device_ptr> devices_;
};

/// Scheduler for multiple devices (homogeneous).
/// Uses a simple random lottery to pick a device.
class multi_device_scheduler : public scheduler {
public:
  void set_devices(const std::vector<device_ptr>& devices) override;
  device_ptr schedule([[maybe_unused]] int actor_id) override;
  device_ptr schedule([[maybe_unused]] int actor_id,
                      int device_number) override;
  void getStreamAndContext(int actor_id, CUcontext* context,
                           CUstream* stream) override;

private:
  device_ptr find_device_by_id(int id) override;

  std::vector<device_ptr> devices_;
};

} // namespace caf::cuda

