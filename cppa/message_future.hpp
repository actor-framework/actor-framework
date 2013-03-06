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


#ifndef MESSAGE_FUTURE_HPP
#define MESSAGE_FUTURE_HPP

#include <cstdint>
#include <type_traits>

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/behavior.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/message_id.hpp"
#include "cppa/local_actor.hpp"

#include "cppa/util/callable_trait.hpp"

namespace cppa {

/**
 * @brief Represents the result of a synchronous send.
 */
class message_future {

 public:

    class continue_helper {

     public:

        inline continue_helper(message_id_t mid) : m_mid(mid) { }

        template<typename F>
        void continue_with(F fun) {
            auto ref_opt = self->bhvr_stack().sync_handler(m_mid);
            if (ref_opt) {
                auto& ref = *ref_opt;
                // copy original behavior
                behavior cpy = ref;
                ref = cpy.add_continuation(std::move(fun));
            }
        }

     private:

        message_id_t m_mid;

    };

    message_future() = delete;

    /**
     * @brief Sets @p mexpr as event-handler for the response message.
     */
    template<typename... Cases, typename... Args>
    continue_helper then(const match_expr<Cases...>& a0, const Args&... as) {
        check_consistency();
        self->become_waiting_for(match_expr_convert(a0, as...), m_mid);
        return {m_mid};
    }

    /**
     * @brief Blocks until the response arrives and then executes @p mexpr.
     */
    template<typename... Cases, typename... Args>
    void await(const match_expr<Cases...>& arg0, const Args&... args) {
        check_consistency();
        self->dequeue_response(match_expr_convert(arg0, args...), m_mid);
    }

    /**
     * @brief Sets @p fun as event-handler for the response message, calls
     *        <tt>self->handle_sync_failure()</tt> if the response message
     *        is an 'EXITED' or 'VOID' message.
     */
    template<typename... Fs>
    typename std::enable_if<
        util::all_callable<Fs...>::value,
        continue_helper
    >::type
    then(Fs... fs) {
        check_consistency();
        self->become_waiting_for(fs2bhvr(std::move(fs)...), m_mid);
        return {m_mid};
    }

    /**
     * @brief Blocks until the response arrives and then executes @p @p fun,
     *        calls <tt>self->handle_sync_failure()</tt> if the response
     *        message is an 'EXITED' or 'VOID' message.
     */
    template<typename... Fs>
    typename std::enable_if<util::all_callable<Fs...>::value>::type
    await(Fs... fs) {
        check_consistency();
        self->dequeue_response(fs2bhvr(std::move(fs)...), m_mid);
    }

    /**
     * @brief Returns the awaited response ID.
     */
    inline const message_id_t& id() const { return m_mid; }

    message_future(const message_future&) = default;
    message_future& operator=(const message_future&) = default;

    inline message_future(const message_id_t& from) : m_mid(from) { }

 private:

    message_id_t m_mid;

    template<typename... Fs>
    behavior fs2bhvr(Fs... fs) {
        auto handle_sync_timeout = []() -> bool {
            self->handle_sync_timeout();
            return false;
        };
        return {
            on(atom("EXITED"), val<std::uint32_t>) >> skip_message,
            on(atom("VOID")) >> skip_message,
            on(atom("TIMEOUT")) >> handle_sync_timeout,
            (on(any_vals, arg_match) >> fs)...
        };
    }

    void check_consistency() {
        if (!m_mid.valid() || !m_mid.is_response()) {
            throw std::logic_error("handle does not point to a response");
        }
        else if (!self->awaits(m_mid)) {
            throw std::logic_error("response already received");
        }
    }

};

class sync_handle_helper {

 public:

    inline sync_handle_helper(const message_future& mf) : m_mf(mf) { }

    template<typename... Args>
    inline message_future::continue_helper operator()(Args&&... args) {
        return m_mf.then(std::forward<Args>(args)...);
    }

 private:

    message_future m_mf;

};

class sync_receive_helper {

 public:

    inline sync_receive_helper(const message_future& mf) : m_mf(mf) { }

    template<typename... Args>
    inline void operator()(Args&&... args) {
        m_mf.await(std::forward<Args>(args)...);
    }

 private:

    message_future m_mf;

};

/**
 * @brief Receives a synchronous response message.
 * @param handle A future for a synchronous response.
 * @throws std::logic_error if @p handle is not valid or if the actor
 *                          already received the response for @p handle
 * @relates message_future
 */
inline sync_handle_helper handle_response(const message_future& f) {
    return {f};
}

/**
 * @brief Handles a synchronous response message in an event-based way.
 * @param handle A future for a synchronous response.
 * @throws std::logic_error if @p handle is not valid or if the actor
 *                          already received the response for @p handle
 * @relates message_future
 */
inline sync_receive_helper receive_response(const message_future& f) {
    return {f};
}

} // namespace cppa

#endif // MESSAGE_FUTURE_HPP
