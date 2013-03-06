/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 * Raphael Hiesgen <raphael.hiesgen@haw-hamburg.de>                           *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/

#include <sstream>
#include <iostream>
#include <stdexcept>

#include "cppa/cppa.hpp"
#include "cppa/opencl/command_dispatcher.hpp"

using namespace std;

namespace cppa { namespace opencl {

struct command_dispatcher::worker {

    command_dispatcher* m_parent;

    typedef command_ptr job_ptr;

    job_queue* m_job_queue;
    thread m_thread;
    job_ptr m_dummy;

    worker(command_dispatcher* parent, job_queue* jq, job_ptr dummy)
        : m_parent(parent), m_job_queue(jq), m_dummy(dummy) { }

    void start() {
        m_thread = thread(&command_dispatcher::worker_loop, this);
    }

    worker(const worker&) = delete;

    worker& operator=(const worker&) = delete;

    void operator()() {
        job_ptr job;
        for (;;) {
            /* wait for device */
            /* get results */
            /* wait for job */
            // adopt reference count of job queue
            job.adopt(m_job_queue->pop());
            if(job != m_dummy) {
                try {
                    cl_command_queue cmd_q =
                            m_parent->m_devices.front().cmd_queue.get();
                    job->enqueue(cmd_q);
                }
                catch (exception& e) {
                    cerr << e.what() << endl;
                }
            }
            else {
                cout << "worker done" << endl;
                return;
            }
            // ...?
        }
    }

};


void command_dispatcher::worker_loop(command_dispatcher::worker* w) {
    (*w)();
}

void command_dispatcher::supervisor_loop(command_dispatcher* scheduler,
                                         job_queue* jq, command_ptr m_dummy) {
    unique_ptr<command_dispatcher::worker> worker;
    worker.reset(new command_dispatcher::worker(scheduler, jq, m_dummy));
    worker->start();
    worker->m_thread.join();
    worker.reset();
    cout << "supervisor done" << endl;
}

void command_dispatcher::initialize() {

    m_dummy = make_counted<command_dummy>();

    cl_int err{0};

    /* find up to two available platforms */
    vector<cl_platform_id> ids(2);
    cl_uint number_of_platforms;
    err = clGetPlatformIDs(ids.size(), ids.data(), &number_of_platforms);
    if (err != CL_SUCCESS) {
        throw logic_error("[!!!] clGetPlatformIDs: '"
                               + get_opencl_error(err)
                               + "'.");
    }
    else if (number_of_platforms < 1) {
        throw logic_error("[!!!] clGetPlatformIDs: 'no platforms found'.");
    }

    /* find gpu devices on our platform */
    int pid{0};
    cl_uint num_devices{0};
    cout << "Currently only looking for cpu devices!" << endl;
    cl_device_type dev_type{CL_DEVICE_TYPE_CPU /*CL_DEVICE_TYPE_GPU*/};
    err = clGetDeviceIDs(ids[pid], dev_type, 0, NULL, &num_devices);
    if (err == CL_DEVICE_NOT_FOUND) {
        cout << "NO GPU DEVICES FOUND! LOOKING FOR CPU DEVICES NOW ..." << endl;
        dev_type = CL_DEVICE_TYPE_CPU;
        err = clGetDeviceIDs(ids[pid], dev_type, 0, NULL, &num_devices);
    }
    if (err != CL_SUCCESS) {
        throw runtime_error("[!!!] clGetDeviceIDs: '"
                                 + get_opencl_error(err)
                                 + "'.");
    }
    vector<cl_device_id> devices(num_devices);
    err = clGetDeviceIDs(ids[pid], dev_type, num_devices, devices.data(), NULL);
    if (err != CL_SUCCESS) {
        throw runtime_error("[!!!] clGetDeviceIDs: '"
                                 + get_opencl_error(err)
                                 + "'.");
    }

    /* create a context */
    m_context.adopt(clCreateContext(0, 1, devices.data(), NULL, NULL, &err));
    if (err != CL_SUCCESS) {
        throw runtime_error("[!!!] clCreateContext: '"
                                 + get_opencl_error(err)
                                 + "'.");
    }

    for (auto& d : devices) {
        device_ptr device;
        device.adopt(d);
        unsigned id{++dev_id_gen};
        command_queue_ptr cmd_queue;
        cmd_queue.adopt(clCreateCommandQueue(m_context.get(),
                                             device.get(),
                                             CL_QUEUE_PROFILING_ENABLE,
                                             &err));
        if (err != CL_SUCCESS) {
            ostringstream oss;
            oss << "[!!!] clCreateCommandQueue (" << id << "): '"
                << get_opencl_error(err) << "'.";
            throw runtime_error(oss.str());
        }
        size_t return_size{0};
        size_t max_work_group_size{0};
        err = clGetDeviceInfo(device.get(),
                              CL_DEVICE_MAX_WORK_GROUP_SIZE,
                              sizeof(size_t),
                              &max_work_group_size,
                              &return_size);
        if (err != CL_SUCCESS) {
            ostringstream oss;
            oss << "[!!!] clGetDeviceInfo ("
                << id
                << ":CL_DEVICE_MAX_WORK_GROUP_SIZE): '"
                << get_opencl_error(err) << "'.";
            throw runtime_error(oss.str());
        }
        cl_uint max_work_item_dimensions = 0;
        err = clGetDeviceInfo(device.get(),
                              CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
                              sizeof(cl_uint),
                              &max_work_item_dimensions,
                              &return_size);
        if (err != CL_SUCCESS) {
            ostringstream oss;
            oss << "[!!!] clGetDeviceInfo ("
                << id
                << ":CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS): '"
                << get_opencl_error(err) << "'.";
            throw runtime_error(oss.str());
        }
        vector<size_t> max_work_items_per_dim(max_work_item_dimensions);
        err = clGetDeviceInfo(device.get(),
                              CL_DEVICE_MAX_WORK_ITEM_SIZES,
                              sizeof(size_t)*max_work_item_dimensions,
                              max_work_items_per_dim.data(),
                              &return_size);
        if (err != CL_SUCCESS) {
            ostringstream oss;
            oss << "[!!!] clGetDeviceInfo ("
                << id
                << ":CL_DEVICE_MAX_WORK_ITEM_SIZES): '"
                << get_opencl_error(err) << "'.";
            throw runtime_error(oss.str());
        }
        device_info dev_info{id,
                             cmd_queue,
                             device,
                             max_work_group_size,
                             max_work_item_dimensions,
                             move(max_work_items_per_dim)};
        m_devices.push_back(move(dev_info));
    }
    m_supervisor = thread(&command_dispatcher::supervisor_loop,
                          this,
                          &m_job_queue,
                          m_dummy);
}

void command_dispatcher::destroy() {
    m_dummy->ref(); // reference of m_job_queue
    m_job_queue.push_back(m_dummy.get());
    m_supervisor.join();
    delete this;
}

void command_dispatcher::dispose() {
    delete this;
}

} } // namespace cppa::opencl
