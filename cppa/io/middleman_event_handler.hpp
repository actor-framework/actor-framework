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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_IO_MIDDLEMAN_EVENT_HANDLER_HPP
#define CPPA_IO_MIDDLEMAN_EVENT_HANDLER_HPP

#include <vector>
#include <utility>

#include "cppa/config.hpp"
#include "cppa/logging.hpp"

#include "cppa/io/event.hpp"
#include "cppa/io/continuable.hpp"

namespace cppa {
namespace io {

struct fd_meta_info {
    native_socket_type fd;
    continuable* ptr;
    event_bitmask mask;
    fd_meta_info(native_socket_type a0,
                 continuable* a1,
                 event_bitmask a2)
    : fd(a0), ptr(a1), mask(a2) { }
};

enum class fd_meta_event { add, erase, mod };

class middleman_event_handler {

 public:

    virtual ~middleman_event_handler();

    /**
     * @brief Enqueues an add operation.
     */
    void add_later(continuable* ptr, event_bitmask e);

    /**
     * @brief Enqueues an erase operation.
     */
    void erase_later(continuable* ptr, event_bitmask e);

    /**
     * @brief Poll all events.
     */
    template<typename F>
    void poll(const F& fun) {
        poll_impl();
        for (auto& p : m_events) fun(p.first, p.second);
        m_events.clear();
        update();
    }

    // pure virtual member function

    virtual void init() = 0;

    inline size_t num_sockets() const { return m_meta.size(); }

    // implemented in platform-dependent .cpp file
    static std::unique_ptr<middleman_event_handler> create();

    /**
     * @brief Performs all actions enqueued by {@link add_later}
     *        or {@link erase_later}.
     */
    void update();

    bool has_reader(continuable* ptr);

    bool has_writer(continuable* ptr);

    template<typename F>
    inline void for_each_reader(F fun) {
        for (auto& meta : m_meta) if (meta.mask & event::read) fun(meta.ptr);
    }

 protected:

    std::vector<fd_meta_info> m_meta; // this vector is *always* sorted

    std::vector<std::pair<fd_meta_info, fd_meta_event>> m_alterations;

    std::vector<std::pair<event_bitmask, continuable*>> m_events;

    std::vector<continuable*> m_dispose_list;

    middleman_event_handler();

    // fills the event vector
    virtual void poll_impl() = 0;

    virtual void handle_event(fd_meta_event me,
                              native_socket_type fd,
                              event_bitmask old_bitmask,
                              event_bitmask new_bitmask,
                              continuable* ptr) = 0;

 private:

    void alteration(continuable* ptr, event_bitmask e, fd_meta_event etype);

    event_bitmask next_bitmask(event_bitmask old, event_bitmask arg, fd_meta_event op) const;

};

} // namespace io
} // namespace cppa

#endif // CPPA_IO_MIDDLEMAN_EVENT_HANDLER_HPP
