#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>

#include <ucontext.h>
#include <sys/mman.h>
#include <signal.h>
#include <stddef.h>

#include "cppa_fibre.h"

/*
struct cppa_fibre_struct
{
    int m_initialized;
    ucontext_t m_context;
    void (*m_fun)();
    void* m_init_arg;
};
*/

void cppa_fibre_ctor(cppa_fibre* instance)
{
    instance->m_state = 0;
    memset(&(instance->m_context), 0, sizeof(ucontext_t));
    getcontext(&(instance->m_context));
    instance->m_fun = 0;
    instance->m_init_arg = 0;
}

void cppa_fibre_ctor2(cppa_fibre* instance, void (*fun)(), void* arg)
{
    cppa_fibre_ctor(instance);
    instance->m_state = 1;
    instance->m_fun = fun;
    instance->m_init_arg = arg;
}

void cppa_fibre_initialize(cppa_fibre* instance)
{
    if (instance->m_state == 1)
    {
        instance->m_state = 2;
        instance->m_context.uc_stack.ss_sp = mmap(0,
                                                  SIGSTKSZ,
                                                  PROT_EXEC | PROT_READ | PROT_WRITE,
                                                  MAP_PRIVATE | MAP_ANON,
                                                  -1,
                                                  0);
        instance->m_context.uc_stack.ss_size = SIGSTKSZ;
        makecontext(&(instance->m_context), instance->m_fun, 0);
    }
}

void cppa_fibre_dtor(cppa_fibre* instance)
{

}

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

