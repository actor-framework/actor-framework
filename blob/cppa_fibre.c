#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <ucontext.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "cppa_fibre.h"

__thread void* s_switch_arg;
__thread int s_yield_value;
__thread ucontext_t* s_caller;
__thread ucontext_t* s_callee;

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
        s_switch_arg = instance->m_init_arg;
    }
}

void cppa_fibre_dtor(cppa_fibre* instance)
{
    if (instance->m_state == 2)
    {
        munmap(instance->m_context.uc_stack.ss_sp, SIGSTKSZ);
    }
}

void* cppa_fibre_init_switch_arg()
{
    return s_switch_arg;
}

void cppa_fibre_switch(cppa_fibre* from, cppa_fibre* to)
{
    ucontext_t* ctx_from = &(from->m_context);
    ucontext_t* ctx_to = &(to->m_context);
    s_caller = ctx_from;
    s_callee = ctx_to;
    swapcontext(ctx_from, ctx_to);
}

void cppa_fibre_yield(int value)
{
    s_yield_value = value;
    swapcontext(s_callee, s_caller);
}

int cppa_fibre_yielded_value()
{
    return s_yield_value;
}
