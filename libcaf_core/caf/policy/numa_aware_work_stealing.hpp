/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_POLICY_NUMA_AWARE_WORK_STEALING_HPP
#define CAF_POLICY_NUMA_AWARE_WORK_STEALING_HPP

#include <deque>
#include <chrono>
#include <thread>
#include <random>
#include <cstddef>

#include <hwloc.h>

#include "caf/policy/work_stealing.hpp"

namespace caf {
namespace policy {

#define CALL_CAF_CRITICAL(predicate, msg)  \
  if (predicate)                           \
    CAF_CRITICAL(msg)

/// Implements scheduling of actors via a numa aware work stealing.
/// @extends scheduler_policy
class numa_aware_work_stealing : public work_stealing {
public:
  ~numa_aware_work_stealing();

  struct hwloc_bitmap_topo_free {
    void operator()(hwloc_topology_t p) {
      hwloc_topology_destroy(p);
    }
  };

  using topo_ptr = std::unique_ptr<hwloc_topology, hwloc_bitmap_topo_free>;

  template <class Worker>
  struct worker_deleter {
    worker_deleter(topo_ptr& t) 
      : topo(t)
    { }
    void operator()(void * p) {
      hwloc_free(topo.get(), p, sizeof(Worker));
    }
    topo_ptr& topo;
  };
  
  struct hwloc_bitmap_free_wrapper {
    void operator()(hwloc_bitmap_t p) {
      hwloc_bitmap_free(p);
    }
  };

  using hwloc_bitmap_wrapper =
    std::unique_ptr<hwloc_bitmap_s, hwloc_bitmap_free_wrapper>;

  static hwloc_bitmap_wrapper hwloc_bitmap_make_wrapper() {
    return hwloc_bitmap_wrapper(hwloc_bitmap_alloc());
  }

  using pu_id_t = int;
  using node_id_t = int;
  using pu_set_t = hwloc_bitmap_wrapper;
  using node_set_t = hwloc_bitmap_wrapper;

  template <class Worker>
  struct coordinator_data {
    inline explicit coordinator_data(scheduler::abstract_coordinator*) {
      int res;
      hwloc_topology_t raw_topo;
      res = hwloc_topology_init(&raw_topo);
      CALL_CAF_CRITICAL(res == -1, "hwloc_topology_init() failed");
      topo.reset(raw_topo);
      res = hwloc_topology_load(topo.get());
      CALL_CAF_CRITICAL(res == -1, "hwloc_topology_load() failed");
      next_worker = 0;
    }
    topo_ptr topo;
    std::vector<std::unique_ptr<Worker, worker_deleter<Worker>>> workers;
    std::map<pu_id_t, Worker*> worker_id_map;
    // used by central enqueue to balance new jobs between workers with round
    // robin strategy
    std::atomic<size_t> next_worker; 
  };

  template <class Worker>
  struct worker_data {
    using neighbors_t = std::vector<Worker*>;
    using worker_matrix_t = std::vector<neighbors_t>;

    explicit worker_data(scheduler::abstract_coordinator* p)
        : rengine(std::random_device{}())
        , strategies(get_poll_strategies(p))
        , neighborhood_level(
            p->system().config().numa_aware_work_stealing_neighborhood_level) {
      // nop
    }

    worker_matrix_t init_worker_matrix(Worker* self,
                                       const pu_set_t& current_pu_id_set) {
      auto& cdata = d(self->parent());
      auto& topo = cdata.topo;
      auto current_node_set = hwloc_bitmap_make_wrapper();
      hwloc_cpuset_to_nodeset(topo.get(), current_pu_id_set.get(),
                              current_node_set.get());
      CALL_CAF_CRITICAL(hwloc_bitmap_iszero(current_node_set.get()),
                        "Current NUMA node_set is unknown");
      auto current_node_id = hwloc_bitmap_first(current_node_set.get());
      std::map<float, pu_set_t> dist_map;
      worker_matrix_t result_matrix;
      // Distance matrix of NUMA nodes.
      // It is possible to request the distance matrix on PU level,
      // which would be a better match for our usecase
      // but on all tested hardware it returned a nullptr, maybe future
      // work?
      auto distance_matrix =
        hwloc_get_whole_distance_matrix_by_type(topo.get(), HWLOC_OBJ_NUMANODE);
      // if NUMA distance matrix is not available it is assumed that all PUs
      // have the same distance
      if (!distance_matrix || !distance_matrix->latency) {
        auto allowed_const_pus = hwloc_topology_get_allowed_cpuset(topo.get());
        hwloc_bitmap_wrapper allowed_pus;
        allowed_pus.reset(hwloc_bitmap_dup(allowed_const_pus));
        dist_map.insert(std::make_pair(1.0, std::move(allowed_pus)));  
      } else {
        auto num_of_dist_objs = distance_matrix->nbobjs;
        // relvant line for the current NUMA node in distance matrix
        float* dist_pointer =
          &distance_matrix
             ->latency[num_of_dist_objs
                       * static_cast<unsigned int>(current_node_id)];
        // iterate over all NUMA nodes and classify them in distance levels
        // regarding to the current NUMA node
        for (node_id_t x = 0; static_cast<unsigned int>(x) < num_of_dist_objs;
             ++x) {
          node_set_t tmp_node_set = hwloc_bitmap_make_wrapper();
          hwloc_bitmap_set(tmp_node_set.get(), static_cast<unsigned int>(x));
          auto tmp_pu_set = hwloc_bitmap_make_wrapper();
          hwloc_cpuset_from_nodeset(topo.get(), tmp_pu_set.get(),
                                    tmp_node_set.get());
          // you cannot steal from yourself
          if (x == current_node_id) {
            hwloc_bitmap_andnot(tmp_pu_set.get(), tmp_pu_set.get(),
                                current_pu_id_set.get());
          }
          auto dist_it = dist_map.find(dist_pointer[x]);
          if (dist_it == dist_map.end())
            // create a new distane level
            dist_map.insert(
              std::make_pair(dist_pointer[x], std::move(tmp_pu_set)));
          else
            // add PUs to an available distance level
            hwloc_bitmap_or(dist_it->second.get(), dist_it->second.get(),
                            tmp_pu_set.get());
        }
      }
      // return PU matrix sorted by its distance
      result_matrix.reserve(dist_map.size());
      for (auto& pu_set_it : dist_map) {
        std::vector<Worker*> current_lvl;
        auto pu_set = pu_set_it.second.get();
        for (pu_id_t pu_id = hwloc_bitmap_first(pu_set); pu_id != -1;
             pu_id = hwloc_bitmap_next(pu_set, pu_id)) {
          auto worker_id_it = cdata.worker_id_map.find(pu_id);
          // if worker id is not found less worker than available PUs
          // have been started
          if (worker_id_it != cdata.worker_id_map.end())
            current_lvl.emplace_back(worker_id_it->second);
        }
        // current_lvl can be empty if all pus of NUMA node are deactivated
        if (!current_lvl.empty()) {
          result_matrix.emplace_back(std::move(current_lvl));
        }
      }
      //accumulate scheduler_lvls - each lvl contains all lower lvls
      auto last_lvl_it = result_matrix.begin();
      for (auto current_lvl_it = result_matrix.begin();
           current_lvl_it != result_matrix.end(); ++current_lvl_it) {
        if (current_lvl_it != result_matrix.begin()) {
          std::copy(last_lvl_it->begin(), last_lvl_it->end(),
                    std::back_inserter(*current_lvl_it));
          ++last_lvl_it;
        }
      } 
      return result_matrix;
    }
  
    // This queue is exposed to other workers that may attempt to steal jobs
    // from it and the central scheduling unit can push new jobs to the queue.
    queue_type queue;
    worker_matrix_t worker_matrix;
    std::default_random_engine rengine;
    std::uniform_int_distribution<size_t> uniform;
    std::vector<poll_strategy> strategies;
    size_t neighborhood_level;
  };

  /// Create x workers.
  template <class Coordinator, class Worker>
  void create_workers(Coordinator* self, size_t num_workers,
                                         size_t throughput) {
    auto& cdata = d(self);
    auto& topo = cdata.topo;
    auto allowed_pus = hwloc_topology_get_allowed_cpuset(topo.get());
    size_t num_allowed_pus =
      static_cast<size_t>(hwloc_bitmap_weight(allowed_pus));
    CALL_CAF_CRITICAL(num_allowed_pus < num_workers,
                      "less PUs than worker");
    cdata.workers.reserve(num_allowed_pus);
    auto pu_set = hwloc_bitmap_make_wrapper();
    auto node_set = hwloc_bitmap_make_wrapper();
    auto pu_id = hwloc_bitmap_first(allowed_pus);
    size_t worker_count = 0;
    while (pu_id != -1 && worker_count < num_workers) {
      hwloc_bitmap_only(pu_set.get(), static_cast<unsigned int>(pu_id));
      hwloc_cpuset_to_nodeset(topo.get(), pu_set.get(), node_set.get());
      auto ptr =
        hwloc_alloc_membind_nodeset(topo.get(), sizeof(Worker), node_set.get(),
                                    HWLOC_MEMBIND_BIND, HWLOC_MEMBIND_THREAD);
      std::unique_ptr<Worker, worker_deleter<Worker>> worker(
        new (ptr) Worker(static_cast<unsigned int>(pu_id), self, throughput),
        worker_deleter<Worker>(topo));
      cdata.worker_id_map.insert(std::make_pair(pu_id, worker.get()));
      cdata.workers.emplace_back(std::move(worker));
      pu_id = hwloc_bitmap_next(allowed_pus, pu_id);
      worker_count++;
    }
  }

  /// Initalize worker thread.
  template <class Worker>
  void init_worker_thread(Worker* self) {
    auto& wdata = d(self);
    auto& cdata = d(self->parent());
    auto pu_set = hwloc_bitmap_make_wrapper();
    hwloc_bitmap_set(pu_set.get(), static_cast<unsigned int>(self->id()));
    auto res = hwloc_set_cpubind(cdata.topo.get(), pu_set.get(),
                          HWLOC_CPUBIND_THREAD | HWLOC_CPUBIND_NOMEMBIND);
    CALL_CAF_CRITICAL(res == -1, "hwloc_set_cpubind() failed");
    wdata.worker_matrix = wdata.init_worker_matrix(self, pu_set);

    auto wm_max_idx = wdata.worker_matrix.size() - 1;
    if (wdata.neighborhood_level == 0) {
      self->set_all_workers_are_neighbors(true); 
    } else if (wdata.neighborhood_level <= wm_max_idx) {
      self->set_neighbors(
        wdata.worker_matrix[wm_max_idx - wdata.neighborhood_level]);
        self->set_all_workers_are_neighbors(false);
    } else { //neighborhood_level > wm_max_idx
        self->set_all_workers_are_neighbors(false);
    }
  }

  template <class Worker>
  resumable* try_steal(Worker* self, size_t& scheduler_lvl_idx,
                       size_t& steal_cnt) {
    //auto p = self->parent();
    auto& wdata = d(self);
    auto& cdata = d(self->parent());
    size_t num_workers = cdata.workers.size();
    if (num_workers < 2) {
      // you can't steal from yourself, can you?
      return nullptr;
    }
    auto& wmatrix = wdata.worker_matrix;
    auto& scheduler_lvl = wmatrix[scheduler_lvl_idx];
    auto res =
      scheduler_lvl[wdata.uniform(wdata.rengine) % scheduler_lvl.size()]
        ->data()
        .queue.take_tail();
    ++steal_cnt;
    if (steal_cnt >= scheduler_lvl.size()) {
      steal_cnt = 0; 
      ++scheduler_lvl_idx;
      if (scheduler_lvl_idx >= wmatrix.size()) {
        scheduler_lvl_idx = wmatrix.size() -1;
      }
    }
    return res;
  }

  template <class Worker>
  resumable* dequeue(Worker* self) {
    // we wait for new jobs by polling our external queue: first, we
    // assume an active work load on the machine and perform aggresive
    // polling, then we relax our polling a bit and wait 50 us between
    // dequeue attempts, finally we assume pretty much nothing is going
    // on and poll every 10 ms; this strategy strives to minimize the
    // downside of "busy waiting", which still performs much better than a
    // "signalizing" implementation based on mutexes and conition variables
    size_t scheduler_lvl_idx = 0;
    size_t steal_cnt = 0;
    auto& strategies = d(self).strategies;
    resumable* job = nullptr;
    for (auto& strat : strategies) {
      for (size_t i = 0; i < strat.attempts; i += strat.step_size) {
        job = d(self).queue.take_head();
        if (job)
          return job;
        // try to steal every X poll attempts
        if ((i % strat.steal_interval) == 0) {
          job = try_steal(self, scheduler_lvl_idx, steal_cnt);
          if (job)
            return job;
        }
        if (strat.sleep_duration.count() > 0)
          std::this_thread::sleep_for(strat.sleep_duration);
      }
    }
    // unreachable, because the last strategy loops
    // until a job has been dequeued
    return nullptr;
  }
private:
  // -- debug stuff --
  friend std::ostream& operator<<(std::ostream& s,
                                  const hwloc_bitmap_wrapper& w);
};


} // namespace policy
} // namespace caf

#endif // CAF_POLICY_NUMA_AWARE_WORK_STEALING_HPP
