/*
            Copyright Oliver Kowalke 2009.
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

/*******************************************************************
 *                                                                 *
 *  -------------------------------------------------------------  *
 *  |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  *
 *  -------------------------------------------------------------  *
 *  | 0x0 | 0x4 | 0x8 | 0xc | 0x10| 0x14| 0x18| 0x1c| 0x20| 0x24|  *
 *  -------------------------------------------------------------  *
 *  |  v1 |  v2 |  v3 |  v4 |  v5 |  v6 |  v7 |  v8 |  sp |  lr |  *
 *  -------------------------------------------------------------  *
 *  -------------------------------------------------------------  *
 *  |  10 |                                                     |  *
 *  -------------------------------------------------------------  *
 *  | 0x28|                                                     |  *
 *  -------------------------------------------------------------  *
 *  |  pc |                                                     |  *
 *  -------------------------------------------------------------  *
 *  -------------------------------------------------------------  *
 *  |  11 |  12 |                                               |  *
 *  -------------------------------------------------------------  *
 *  | 0x2c| 0x30|                                               |  *
 *  -------------------------------------------------------------  *
 *  |sbase|slimit|                                              |  *
 *  -------------------------------------------------------------  *
 *  -------------------------------------------------------------  *
 *  |  13 |  14 | 15 |  16 |  17 |  18 |  19 |  20 |  21 |  22  |  *
 *  -------------------------------------------------------------  *
 *  | 0x34| 0x38|0x3c| 0x40| 0x44| 0x48| 0x4c| 0x50| 0x54| 0x58 |  *
 *  -------------------------------------------------------------  *
 *  | s16 | s17 | s18 | s19 | s20 | s21 | s22 | s23 | s24 | s25 |  *
 *  -------------------------------------------------------------  *
 *  -------------------------------------------------------------  *
 *  |  23 |  24 |  25 |  26 |  27 |  28 |                       |  *
 *  -------------------------------------------------------------  *
 *  | 0x5c| 0x60| 0x64| 0x68| 0x6c| 0x70|                       |  *
 *  -------------------------------------------------------------  *
 *  | s26 | s27 | s28 | s29 | s30 | s31 |                       |  *
 *  -------------------------------------------------------------  *
 *                                                                 *
 * *****************************************************************/

.text
.globl jump_fcontext
.align 2
.type jump_fcontext,%function
jump_fcontext:
    stmia   a1, {v1-v8,sp-lr}       @ save V1-V8,SP-LR
    str     lr, [a1,#40]            @ save LR as PC
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
    cmp     a4, #0                  @ test if fpu env should be preserved
    be      1f

    ldr     a4, [a1,#52]
    stmia   a4, {s16-s31}           @ save S16-S31

    ldr     a4, [a2,#52]
    ldmia   a4, {s16-s31}           @ restore S16-S31
1:
#endif

	mov		a1, a3                  @ use third arg as return value after jump
	                                @ and as first arg in context function
    ldmia   a2, {v1-v8,sp-pc}       @ restore v1-V8,SP-PC
.size jump_fcontext,.-jump_fcontext

.text
.globl make_fcontext
.align 2
.type make_fcontext,%function
make_fcontext:
    str     a1, [a1,#0]         @ save the address of passed context
    str     a2, [a1,#40]        @ save address of the context function
    ldr     a2, [a1,#44]        @ load the stack base

	push    {a1,lr}				@ save pointer to fcontext_t
	mov     a1, a2				@ stack pointer as arg for align_stack
	bl		align_stack@PLT     @ align stack
	mov		a2, a1				@ begin of aligned stack 
	pop     {a1,lr}				@ restore pointer to fcontext_t

    str     a2, [a1,#32]        @ save the aligned stack base

    adr     a2, finish          @ address of finish; called after context function returns
    str     a2, [a1,#36]

    mov     a1, #0
    bx      lr

finish:
    mov     a1, #0              @ exit code is zero
    bl      _exit@PLT           @ exit application
.size make_fcontext,.-make_fcontext
