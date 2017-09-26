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

#ifndef CAF_POLICY_LOCALITY_GUIDED_SCHEDULING_HPP_
#define CAF_POLICY_LOCALITY_GUIDED_SCHEDULING_HPP_

#include <deque>
#include <chrono>
#include <thread>
#include <random>
#include <cstddef>

#include <hwloc.h>

#include "caf/policy/work_stealing.hpp"

namespace caf {
namespace policy {

/// Implements scheduling of actors via a numa aware work stealing.
/// @extends scheduler_policy
class locality_guided_scheduling : public work_stealing {
public:
  ~locality_guided_scheduling();

  struct hwloc_topo_free {
    void operator()(hwloc_topology_t p) {
      hwloc_topology_destroy(p);
    }
  };

  using topo_ptr = std::unique_ptr<hwloc_topology, hwloc_topo_free>;

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

  using bitmap_wrapper_t =
    std::unique_ptr<hwloc_bitmap_s, hwloc_bitmap_free_wrapper>;

  static bitmap_wrapper_t hwloc_bitmap_make_wrapper() {
    return bitmap_wrapper_t(hwloc_bitmap_alloc());
  }

  template <class Worker>
  struct coordinator_data {
    inline explicit coordinator_data(scheduler::abstract_coordinator*) {
      int res;
      hwloc_topology_t raw_topo;
      res = hwloc_topology_init(&raw_topo);
      // hwloc_topology_init() failed
      CAF_IGNORE_UNUSED(res);
      CAF_ASSERT(res == -1);
      topo.reset(raw_topo);
      res = hwloc_topology_load(topo.get());
      // hwloc_topology_load() failed
      CAF_ASSERT(res == -1);
      next_worker = 0;
    }
    topo_ptr topo;
    std::vector<std::unique_ptr<Worker, worker_deleter<Worker>>> workers;
    std::map<int, Worker*> worker_id_map;
    // used by central enqueue to balance new jobs between workers with round
    // robin strategy
    std::atomic<size_t> next_worker; 
  };

  template <class Worker>
  struct worker_data {
    using neighbors_t = std::vector<Worker*>;
    using worker_proximity_matrix_t = std::vector<neighbors_t>;
    using pu_distance_map_t = std::map<float, bitmap_wrapper_t>;

    explicit worker_data(scheduler::abstract_coordinator* p)
        : rengine(std::random_device{}())
        , strategies(get_poll_strategies(p))
        , actor_pinning_entity(p->system().config().lgs_actor_pinning_entity)
        , wws_start_entity(
            p->system().config().lgs_weighted_work_stealing_start_entity)
        , start_steal_group_idx(0) {
      // nop
    }

    //debug fun
    bool check_pu_id(hwloc_const_cpuset_t current_pu) {
      //auto current_pu_id = hwloc_bitmap_first(current_pu);
      //return current_pu_id == 0;
      return false;
    }

    //debug fun
    void xxx(hwloc_const_cpuset_t current_pu, const std::string& str) {
      //if (!check_pu_id(current_pu))
        //return;
      //std::cout << str << std::endl; 
    }

    //debug fun
    void xxx(hwloc_const_bitmap_t current_pu, std::map<float, bitmap_wrapper_t>& dist_map) {
      //if (!check_pu_id(current_pu))
        //return;
      //for(auto& e : dist_map) {
        //std::cout << "dist: " << e.first << "; pu_set: " << e.second << std::endl;
      //}
    }

    // collects recursively all PUs which are children of obj and obj itself
    void traverse_hwloc_obj(hwloc_cpuset_t result_pu_set, hwloc_topology_t topo,
                            const hwloc_obj_t obj, unsigned int filter_os_idx,
                            const hwloc_obj_t filter_obj) {
      if (!obj || obj == filter_obj)
        return;
      if (obj->type == hwloc_obj_type_t::HWLOC_OBJ_PU
          && obj->os_index != filter_os_idx) {
        hwloc_bitmap_set(result_pu_set, obj->os_index);
      } else {
        hwloc_obj_t child = hwloc_get_next_child(topo, obj, nullptr);
        while (child) {
          traverse_hwloc_obj(result_pu_set, topo, child, filter_os_idx,
                             filter_obj);
          child = hwloc_get_next_child(topo, obj, child);
        }
      }
    }

    // collect the PUs for each cache level
    pu_distance_map_t traverse_caches(hwloc_topology_t topo,
                                      hwloc_const_cpuset_t current_pu) {
      pu_distance_map_t result_map;
      // We need the distance devider to define the distance between PUs sharing
      // a cache level. PUs sharing a NUMA-node have a distance of 1 by
      // definition. PUs which don't share a NUMA-node have a distance of > 1.
      // Consequently, a the distance between PUs sharing a cache level must be
      // smaller than 1. We define the distance between PUs sharing the L1 cache
      // as 1 / 100 (the distance_divider). Ergo the distance for the L2 cache
      // is 2 / 100, and so on.  Why 100?: It is readable by humans and at least
      // 100 cache levels are requried to collide with NUMA distances which is
      // very unlikely.
      const float distance_divider = 100.0;
      int current_cache_lvl = 1;
      hwloc_obj_t last_cache_obj = nullptr;
      auto current_cache_obj =
        hwloc_get_cache_covering_cpuset(topo, current_pu);
      auto current_pu_id = hwloc_bitmap_first(current_pu);
      while (current_cache_obj
             && current_cache_obj->type == hwloc_obj_type_t::HWLOC_OBJ_CACHE) {
        auto result_pus = hwloc_bitmap_make_wrapper();
        traverse_hwloc_obj(result_pus.get(), topo, current_cache_obj, current_pu_id,
                           last_cache_obj);
        if (!hwloc_bitmap_iszero(result_pus.get())) {
          result_map.insert(make_pair(current_cache_lvl / distance_divider,
                                      move(result_pus)));
        }
        ++current_cache_lvl;
        last_cache_obj = current_cache_obj;
        current_cache_obj = current_cache_obj->parent;
      }
      return result_map;
    }

    pu_distance_map_t traverse_nodes(hwloc_topology_t topo,
                               const hwloc_distances_s* node_dist_matrix,
                               hwloc_const_cpuset_t current_pu,
                               hwloc_const_cpuset_t current_node) {
      pu_distance_map_t result_map;
      auto current_node_id = hwloc_bitmap_first(current_node);
      auto num_nodes = node_dist_matrix->nbobjs;
      // relvant line for the current NUMA node in distance matrix
      float* dist_ptr =
        &node_dist_matrix
           ->latency[num_nodes * static_cast<unsigned int>(current_node_id)];
      // iterate over all NUMA nodes and classify them in distance levels
      // regarding to the current NUMA node
      for (int x = 0; static_cast<unsigned int>(x) < num_nodes; ++x) {
        auto tmp_nodes = hwloc_bitmap_make_wrapper();
        hwloc_bitmap_set(tmp_nodes.get(), static_cast<unsigned int>(x));
        auto tmp_pus = hwloc_bitmap_make_wrapper();
        hwloc_cpuset_from_nodeset(topo, tmp_pus.get(),
                                  tmp_nodes.get());
        // you cannot steal from yourself
        if (x == current_node_id) {
          hwloc_bitmap_andnot(tmp_pus.get(), tmp_pus.get(),
                              current_pu);
        }
        if (hwloc_bitmap_iszero(tmp_pus.get())) {
          continue; 
        }
        auto result_map_it = result_map.find(dist_ptr[x]);
        if (result_map_it == result_map.end()) {
          // create a new distane group
          result_map.insert(make_pair(dist_ptr[x], move(tmp_pus)));
        } else {
          // add PUs to an available distance group
          hwloc_bitmap_or(result_map_it->second.get(),
                          result_map_it->second.get(), tmp_pus.get());
        }
      }
      return result_map;
    }

    // Merge the distance maps.
    // The pu maps in cache_dists and node_dists must have no set intersections
    // because they are accumulated later.
    // wp_matrix_first_node_idx is set to the first index which represents a
    // full NUMA-node
    pu_distance_map_t merge_dist_maps(pu_distance_map_t&& cache_dists,
                                      pu_distance_map_t&& node_dists,
                                      int& wp_matrix_first_node_idx) {
      if (!cache_dists.empty() && !node_dists.empty()) {
        auto local_node_it = node_dists.begin();
        // remove all pus collected in cache_dists from the local node
        auto local_node = local_node_it->second.get();
        for (auto& e : cache_dists) {
          hwloc_bitmap_andnot(local_node, local_node, e.second.get());
        }
        wp_matrix_first_node_idx = cache_dists.size();
        if (hwloc_bitmap_iszero(local_node)) {
          node_dists.erase(local_node_it);
          --wp_matrix_first_node_idx;
        }
        cache_dists.insert(make_move_iterator(begin(node_dists)),
                             make_move_iterator(end(node_dists)));
        return move(cache_dists);
      } else if (!cache_dists.empty() && node_dists.empty()) {
        // caf cannot it collected all pus because because it CPU could have two
        // L3-caches and only of them is represented by cahces_dists.
        CAF_CRITICAL("caf could not reliable collect all PUs");
      } else if (cache_dists.empty() && !node_dists.empty()) {
        wp_matrix_first_node_idx = 0;
        return move(node_dists);
      } else { 
        // both maps are empty, which happens on a single core machine
        wp_matrix_first_node_idx = -1;
        return pu_distance_map_t{};
      }
    }

    worker_proximity_matrix_t
    init_worker_proximity_matrix(Worker* self,
                                 hwloc_const_cpuset_t current_pu) {
      auto& cdata = d(self->parent());
      auto topo = cdata.topo.get();
      auto current_node = hwloc_bitmap_make_wrapper();
      auto current_pu_id = hwloc_bitmap_first(current_pu);
      hwloc_cpuset_to_nodeset(topo, current_pu,
                              current_node.get());
      // Current NUMA-node is unknown
      CAF_ASSERT(hwloc_bitmap_iszero(current_node.get()));
      pu_distance_map_t pu_dists;
      worker_proximity_matrix_t result_wp_matrix;
      auto node_dist_matrix =
        hwloc_get_whole_distance_matrix_by_type(topo, HWLOC_OBJ_NUMANODE);
      // If NUMA distance matrix we still try to exploit cache locality
      if (!node_dist_matrix || !node_dist_matrix->latency) {
        auto cache_dists = traverse_caches(topo, current_pu);
        // We have to check whether dist_map includes all pus or not.
        // If not, we have to add an additional group which includes them.
        auto all_pus = hwloc_bitmap_make_wrapper();
        const float normalized_numa_node_dist = 1.0;
        for (auto& e: cdata.worker_id_map) {
          if (e.first != current_pu_id) {
            hwloc_bitmap_set(all_pus.get(), e.first);
          }
        }
        pu_distance_map_t tmp_node_dists;
        tmp_node_dists.insert(
          make_pair(normalized_numa_node_dist, move(all_pus)));
        pu_dists = merge_dist_maps(move(cache_dists), move(tmp_node_dists),
                                   wp_matrix_first_node_idx);
      } else {
        auto cache_dists = traverse_caches(topo, current_pu);
        auto node_dists = traverse_nodes(topo, node_dist_matrix, current_pu,
                                         current_node.get());
        pu_dists = merge_dist_maps(move(cache_dists), move(node_dists),
                                   wp_matrix_first_node_idx);
      }
      xxx(current_pu, pu_dists);
      // map PU ids to worker* sorted by its distance
      result_wp_matrix.reserve(pu_dists.size());
      for (auto& pu_set_it : pu_dists) {
        std::vector<Worker*> current_worker_group;
        auto pu_set = pu_set_it.second.get();
        for (int pu_id = hwloc_bitmap_first(pu_set); pu_id != -1;
             pu_id = hwloc_bitmap_next(pu_set, pu_id)) {
          auto worker_id_it = cdata.worker_id_map.find(pu_id);
          // if worker id is not found less worker than available PUs.
          // have been started
          if (worker_id_it != cdata.worker_id_map.end())
            current_worker_group.emplace_back(worker_id_it->second);
        }
        // current_worker_group can be empty if pus of this level are deactivated
        if (!current_worker_group.empty()) {
          result_wp_matrix.emplace_back(move(current_worker_group));
        }
      }
      //accumulate steal_groups - each group contains all lower level groups 
      auto last_group_it = result_wp_matrix.begin();
      for (auto current_group_it = result_wp_matrix.begin();
           current_group_it != result_wp_matrix.end(); ++current_group_it) {
        if (current_group_it != result_wp_matrix.begin()) {
          std::copy(last_group_it->begin(), last_group_it->end(),
                    std::back_inserter(*current_group_it));
          ++last_group_it;
        }
      }

      if (check_pu_id(current_pu)) {
        int distance_idx = 0;
        std::cout << "wp_matrix_first_node_idx: " << wp_matrix_first_node_idx << std::endl;
        for (auto& neighbors : result_wp_matrix) {
          std::cout << "result_matix distance_idx: " << distance_idx++ << std::endl;
          std::cout << " -- ";  
          for (auto neighbor : neighbors) {
            std::cout << neighbor->to_string() << "; ";
          }
          std::cout << std::endl;
        }
      }
      return result_wp_matrix;
    }
  
    // This queue is exposed to other workers that may attempt to steal jobs
    // from it and the central scheduling unit can push new jobs to the queue.
    queue_type queue;
    worker_proximity_matrix_t wp_matrix;
    // Defines the index in wp_matrix which references the local NUMA-node.
    // wp_matrix_first_node_idx is -1 if no neigbhors exist (wp_matrix.empty()).
    int wp_matrix_first_node_idx;
    std::default_random_engine rengine;
    std::uniform_int_distribution<size_t> uniform;
    std::vector<poll_strategy> strategies;
    atom_value actor_pinning_entity;
    atom_value wws_start_entity;
    size_t start_steal_group_idx;
    size_t num_of_steal_attempts = 0;
    size_t num_of_successfully_steals = 0;
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
    // less PUs than worker
    CAF_ASSERT(num_allowed_pus < num_workers);
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
    auto current_pu = hwloc_bitmap_make_wrapper();
    hwloc_bitmap_set(current_pu.get(),
                     static_cast<unsigned int>(self->id()));
    auto res = hwloc_set_cpubind(cdata.topo.get(), current_pu.get(),
                          HWLOC_CPUBIND_THREAD | HWLOC_CPUBIND_NOMEMBIND);
    // hwloc_set_cpubind() failed
    CAF_IGNORE_UNUSED(res);
    CAF_ASSERT(res == -1);
    wdata.wp_matrix = wdata.init_worker_proximity_matrix(self, current_pu.get());
    auto& node_idx = wdata.wp_matrix_first_node_idx;
    auto& wp_matrix = wdata.wp_matrix;
    if (wp_matrix.empty()) {
      // no neighbors could be found, use the fallback behavior
      self->set_all_workers_are_neighbors(true); 
      wdata.xxx(current_pu.get(), "pinnning: wp_matrix.empty(); all are neigbhors");
    } else if (wdata.actor_pinning_entity == atom("pu")) {
        wdata.xxx(current_pu.get(), "pinning: pu; no workers are neigbors");
        self->set_all_workers_are_neighbors(false); 
    } else if (wdata.actor_pinning_entity == atom("cache")) {
      if (wp_matrix.size() == 1) {
        wdata.xxx(current_pu.get(), "pinning: cache; all are neigbhors");
        self->set_all_workers_are_neighbors(true); 
      } else {
        wdata.xxx(current_pu.get(), "pinning: cache; wp_matrix[0]");
        self->set_neighbors(wp_matrix[0]);
        self->set_all_workers_are_neighbors(false); 
      }
    } else if (wdata.actor_pinning_entity == atom("node")) {
      if (node_idx == static_cast<int>(wp_matrix.size()) - 1) {
        wdata.xxx(current_pu.get(), "pinning: node; all are neighbors");
        self->set_all_workers_are_neighbors(true); 
      } else {
        wdata.xxx(current_pu.get(), "pinning: node; wp_matrix[node_idx]");
        self->set_neighbors(wp_matrix[node_idx]);
        self->set_all_workers_are_neighbors(false); 
      }
    } else if (wdata.actor_pinning_entity == atom("system")) {
      wdata.xxx(current_pu.get(), "pinning: system; all are neigbors");
      self->set_all_workers_are_neighbors(true); 
    } else {
      CAF_CRITICAL("config variable actor_pnning_entity with unsopprted value");
    }
    if (wdata.wws_start_entity == atom("cache")) {
      wdata.xxx(current_pu.get(), "wws: cache; start_steal_group_idx  = 0");
      wdata.start_steal_group_idx  = 0;
    } else if (wdata.wws_start_entity == atom("node")) {
      wdata.xxx(current_pu.get(), "wws: node; start_steal_group_idx  = node_idx");
      wdata.start_steal_group_idx  = node_idx;
    } else if (wdata.wws_start_entity == atom("system")) {
      wdata.xxx(current_pu.get(), "wws: system; start_steal_group_idx  = wp_matrix.size() - 1");
      wdata.start_steal_group_idx  = wp_matrix.size() - 1;
    } else {
      CAF_CRITICAL("config variable wws_start_entity with unsopprted value");
    }
  }

  template <class Worker>
  resumable* try_steal(Worker* self, size_t& steal_group_idx,
                       size_t& steal_cnt) {
    auto& wdata = d(self);
    auto& wp_matrix = wdata.wp_matrix;
    if (wp_matrix.empty()) {
      // you can't steal from yourself, can you?
      return nullptr;
    }
    auto& steal_group = wp_matrix[steal_group_idx];
    auto res =
      steal_group[wdata.uniform(wdata.rengine) % steal_group.size()]
        ->data()
        .queue.take_tail();
    ++steal_cnt;
    if (steal_cnt >= steal_group.size()) {
      steal_cnt = 0; 
      ++steal_group_idx;
      if (steal_group_idx >= wp_matrix.size()) {
        steal_group_idx = wp_matrix.size() -1;
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
    size_t steal_group_idx = d(self).start_steal_group_idx;
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
          job = try_steal(self, steal_group_idx, steal_cnt);
          ++d(self).num_of_steal_attempts;
          if (job) {
            ++d(self).num_of_successfully_steals;
            return job;
          }
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
                                  const bitmap_wrapper_t& w);
};


} // namespace policy
} // namespace caf

#endif // CAF_POLICY_LOCALITY_GUIDED_SCHEDULING_HPP_
