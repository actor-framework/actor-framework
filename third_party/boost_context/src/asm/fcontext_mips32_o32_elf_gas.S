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
 *  |  0  |  8  |  16 |  24 |  32 |  40 |  48 |  56 |  64 |  72 |  *
 *  -------------------------------------------------------------  *
 *  |  S0 |  S1 |  S2 |  S3 |  S4 |  S5 |  S6 |  S7 |  GP |  SP |  *
 *  -------------------------------------------------------------  *
 *  -------------------------------------------------------------  *
 *  |  10 |  11 |  12 |                                         |  *
 *  -------------------------------------------------------------  *
 *  |  80 |  88 |  96 |                                         |  *
 *  -------------------------------------------------------------  *
 *  |  S8 |  RA |  PC |                                         |  *
 *  -------------------------------------------------------------  *
 *  -------------------------------------------------------------  *
 *  |  13 |  14 |                                               |  *
 *  -------------------------------------------------------------  *
 *  | 104 | 112 |                                               |  *
 *  -------------------------------------------------------------  *
 *  |sbase|slimt|                                               |  *
 *  -------------------------------------------------------------  *
 *  -------------------------------------------------------------  *
 *  |  15 |  16 |  17 |  18 |  19 |  20 |                       |  *
 *  -------------------------------------------------------------  *
 *  | 120 | 128 | 136 | 144 | 152 | 160 |                       |  *
 *  -------------------------------------------------------------  *
 *  | F20 | F22 | F24 | F26 | F28 | F30 |                       |  *
 *  -------------------------------------------------------------  *
 *                                                                 *
 * *****************************************************************/

.text
.globl jump_fcontext
.align 2
.type jump_fcontext,@function
.ent jump_fcontext
jump_fcontext:
    sw      $s0, ($a0)      # save S0
    sw      $s1, 8($a0)     # save S1
    sw      $s2, 16($a0)    # save S2
    sw      $s3, 24($a0)    # save S3
    sw      $s4, 32($a0)    # save S4
    sw      $s5, 40($a0)    # save S5
    sw      $s6, 48($a0)    # save S6
    sw      $s7, 56($a0)    # save S7
    sw      $gp, 64($a0)    # save GP
    sw      $sp, 72($a0)    # save SP
    sw      $s8, 80($a0)    # save S8
    sw      $ra, 88($a0)    # save RA
    sw      $ra, 96($a0)    # save RA as PC

#if defined(__mips_hard_float)
    beqz    $a3,  1f        # test if fpu env should be preserved 
    s.d     $f20, 120($a0)  # save F20
    s.d     $f22, 128($a0)  # save F22
    s.d     $f24, 136($a0)  # save F24
    s.d     $f26, 144($a0)  # save F26
    s.d     $f28, 152($a0)  # save F28
    s.d     $f30, 160($a0)  # save F30

    l.d     $f20, 120($a1)  # restore F20
    l.d     $f22, 128($a1)  # restore F22
    l.d     $f24, 136($a1)  # restore F24
    l.d     $f26, 144($a1)  # restore F26
    l.d     $f28, 152($a1)  # restore F28
    l.d     $f30, 160($a1)  # restore F30
1:
#endif

    lw      $s0, ($a1)      # restore S0
    lw      $s1, 8($a1)     # restore S1
    lw      $s2, 16($a1)    # restore S2
    lw      $s3, 24($a1)    # restore S3
    lw      $s4, 32($a1)    # restore S4
    lw      $s5, 40($a1)    # restore S5
    lw      $s6, 48($a1)    # restore S6
    lw      $s7, 56($a1)    # restore S7
    lw      $gp, 64($a1)    # restore GP
    lw      $sp, 72($a1)    # restore SP
    lw      $s8, 80($a1)    # restore S8
    lw      $ra, 88($a1)    # restore RA
    move    $a0, $s2        # restore void pointer as argument

    move    $v0, $a2        # use third arg as return value after jump
    move    $a0, $a2        # use third arg as first arg in context function

    lw      $t9, 96($a1)    # load PC
    jr      $t9             # jump to context
.end jump_fcontext
.size jump_fcontext, .-jump_fcontext

.text
.globl make_fcontext
.align 2
.type make_fcontext,@function
.ent make_fcontext
make_fcontext:
#ifdef __PIC__
.set    noreorder
.cpload $t9
.set    reorder
#endif
    sw      $a0, ($a0)          # save the current context
    sw      $gp, 24($a0)        # save global pointer
    sw      $a1, 96($a0)        # save the address of the context function
    lw      $t0, 104($a0)       # load the stack base

    sub     $sp, $sp, 28
    sw      $ra, 24($sp)
    sw      $a0, 20($sp)
    move    $a0, $t0            # stack pointer as arg for align_stack
    lw      $t9, %call16(align_stack)($gp) # align stack
    jalr    $t9
    nop
    move    $t0, $v0            # begin of aligned stack 
    lw      $ra, 24($sp)
    lw      $a0, 20($sp)
    addi    $sp, $sp, 28

    sub     $t0, $t0, 16        # reserve 16 byte of argument space
    sw      $t0, 72($a0)        # save the algned stack base

    la      $t9, finish         # helper code executed after context function returns
    sw      $t9, 88($a0)

    move    $v0, $zero
    jr      $ra

finish:
    move    $gp, $s3            # restore GP (global pointer)
    move    $a0, $zero          # exit code is zero
    lw      $t9, %call16(_exit)($gp)  # exit application
    jalr    $t9
.end make_fcontext
.size make_fcontext, .-make_fcontext
