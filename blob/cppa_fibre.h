#ifndef CPPA_FIBRE_H
#define CPPA_FIBRE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <ucontext.h>

struct cppa_fibre_struct
{
    // 0: *this* context
    // 1: fibre with function to execute, no stack assigned yet
    // 2: as 1 but with assigned stack
    int m_state;
    ucontext_t m_context;
    void (*m_fun)();
    void* m_init_arg;
};

typedef struct cppa_fibre_struct cppa_fibre;

void cppa_fibre_ctor(cppa_fibre* instance);

/*
 * @brief Initializes the given fibre.
 * @param instance Pointer to an uninitialized object.
 * @param fun Function this fibre should execute.
 * @param switch_arg This pointer is stored in a
 *                   thread-local variable on first
 *                   context switch to @p instance.
 */
void cppa_fibre_ctor2(cppa_fibre* instance,
                      void (*fun)(),
                      void* switch_arg);

/*
 * @warning call directly before the first switch
 */
void cppa_fibre_initialize(cppa_fibre* instance);

void cppa_fibre_dtor(cppa_fibre* instance);

/*
 * @brief Returns
 */
void* cppa_fibre_init_switch_arg();

void cppa_fibre_switch(cppa_fibre* from, cppa_fibre* to);

/*
 * Switches back to the calling fibre.
 */
void cppa_fibre_yield(int value);

/*
 * Gets the yielded value of the client fibre.
 */
int cppa_fibre_yielded_value();

#ifdef __cplusplus
}
#endif

#endif

