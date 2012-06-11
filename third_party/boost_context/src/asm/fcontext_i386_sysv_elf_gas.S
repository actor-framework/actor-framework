/*
            Copyright Oliver Kowalke 2009.
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

/********************************************************************
 *                                                                  *
 *  --------------------------------------------------------------  *
 *  |    0    |    1    |    2    |    3    |    4     |    5    |  *
 *  --------------------------------------------------------------  *
 *  |   0x0   |   0x4   |   0x8   |   0xc   |   0x10   |   0x14  |  *
 *  --------------------------------------------------------------  *
 *  |   EDI   |   ESI   |   EBX   |   EBP   |   ESP    |   EIP   |  *
 *  --------------------------------------------------------------  *
 *  --------------------------------------------------------------  *
 *  |    6    |    7    |                                        |  *
 *  --------------------------------------------------------------  *
 *  |   0x18  |   0x1c  |                                        |  *
 *  --------------------------------------------------------------  *
 *  |  sbase  | slimit  |                                        |  *
 *  --------------------------------------------------------------  *
 *  --------------------------------------------------------------  *
 *  |    8    |    9    |                                        |  *
 *  --------------------------------------------------------------  *
 *  |   0x20  |   0x24  |                                        |  *
 *  --------------------------------------------------------------  *
 *  | fc_mxcsr|fc_x87_cw|                                        |  *
 *  --------------------------------------------------------------  *
 *                                                                  *
 * *****************************************************************/

.text
.globl jump_fcontext
.align 2
.type jump_fcontext,@function
jump_fcontext:
    movl    0x4(%esp), %ecx         /* load address of the first fcontext_t arg */
    movl    %edi,       (%ecx)      /* save EDI */
    movl    %esi,       0x4(%ecx)   /* save ESI */
    movl    %ebx,       0x8(%ecx)   /* save EBX */
    movl    %ebp,       0xc(%ecx)   /* save EBP */

    leal    0x4(%esp),  %eax        /* exclude the return address */
    movl    %eax,       0x10(%ecx)  /* save as stack pointer */
    movl    (%esp),     %eax        /* load return address */
    movl    %eax,       0x14(%ecx)  /* save return address */

    movl    0x8(%esp),   %edx       /* load address of the second fcontext_t arg */
    movl    (%edx),      %edi       /* restore EDI */
    movl    0x4(%edx),   %esi       /* restore ESI */
    movl    0x8(%edx),   %ebx       /* restore EBX */
    movl    0xc(%edx),   %ebp       /* restore EBP */

    movl    0x10(%esp),  %eax       /* check if fpu enve preserving was requested */
    test    %eax,        %eax
    je      1f

    stmxcsr  0x20(%ecx)             /* save MMX control and status word */
    fnstcw   0x24(%ecx)             /* save x87 control word */
    ldmxcsr  0x20(%edx)             /* restore MMX control and status word */
    fldcw    0x24(%edx)             /* restore x87 control word */
1:
    movl    0xc(%esp),   %eax       /* use third arg as return value after jump */

    movl    0x10(%edx),  %esp       /* restore ESP */
    movl    %eax,        0x4(%esp)  /* use third arg as first arg in context function */
    movl    0x14(%edx),  %edx       /* fetch the address to return to */

    jmp     *%edx                   /* indirect jump to context */
.size jump_fcontext,.-jump_fcontext

.text
.globl make_fcontext
.align 2
.type make_fcontext,@function
make_fcontext:
    movl   0x4(%esp),      %eax         /* load address of the fcontext_t */
    movl   %eax,           (%eax)       /* save the address of current context */
    movl   0x8(%esp),      %ecx         /* load the address of the context function */
    movl   %ecx,           0x14(%eax)   /* save the address of the context function */
    movl   0x18(%eax),     %edx         /* load the stack base */

    pushl  %eax                         /* save pointer to fcontext_t */
    pushl  %ebx                         /* save EBX */
    pushl  %edx                         /* stack pointer as arg for align_stack */
    call   1f
1:  popl   %ebx                         /* address of label 1 */
    addl   $_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx     /* compute address of GOT and store it in EBX */
    call   align_stack@PLT              /* align stack */
    movl   %eax,           %edx         /* begin of aligned stack */
    popl   %eax                         /* remove arg for align_stack */
    popl   %ebx                         /* restore EBX */
    popl   %eax                         /* restore pointer to fcontext_t */

    leal   -0x14(%edx),    %edx         /* reserve space for the last frame on stack, (ESP + 4) % 16 == 0 */
    movl   %edx,           0x10(%eax)   /* save the aligned stack base */

    stmxcsr  0x20(%eax)                 /* save MMX control and status word */
    fnstcw   0x24(%eax)                 /* save x87 control word */

    call    2f
2:  popl    %ecx                        /* address of label 2 */
    addl    $finish-2b, %ecx            /* helper code executed after context function returns */
    movl    %ecx, (%edx)

    xorl   %eax,            %eax
    ret

finish:
    leal   -0xc(%esp),    %esp

    call    3f
3:  popl    %ebx                                    /* address of label 3 */
    addl    $_GLOBAL_OFFSET_TABLE_+[.-3b], %ebx     /* compute address of GOT and store it in EBX */

    xorl    %eax,           %eax
    pushl   %eax                        /* exit code is zero */
    call   _exit@PLT                    /* exit application */
    hlt
.size make_fcontext,.-make_fcontext
