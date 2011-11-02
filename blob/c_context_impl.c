#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>

#include <ucontext.h>
#include <sys/mman.h>
#include <signal.h>
#include <stddef.h>

#include "cppa_fibre.h"

void coroutine()
{
    for (;;)
    {
        ++m_count;
        printf("m_count = %i\n", m_count);
        swapcontext(&ctx[1], &ctx[0]);
    }
}

int main(int argc, char** argv)
{
    int i;
    void* coroutine_stack = mmap(0,
                                 SIGSTKSZ,
                                 PROT_EXEC | PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANON,
                                 -1,
                                 0);

    memset(&ctx[0], 0, sizeof(ucontext_t));
    getcontext(&ctx[0]);

    memset(&ctx[1], 0, sizeof(ucontext_t));
    getcontext(&ctx[1]);
    ctx[1].uc_stack.ss_sp = coroutine_stack;
    ctx[1].uc_stack.ss_size = SIGSTKSZ;
    ctx[1].uc_link = &ctx[0];
    makecontext(&ctx[1], coroutine, 0);

    for (i = 0; i < 11; ++i)
    {
        printf("i = %i\n", i);
        swapcontext(&ctx[0], &ctx[1]);
    }

    munmap(coroutine_stack, SIGSTKSZ);

    return 0;
}

