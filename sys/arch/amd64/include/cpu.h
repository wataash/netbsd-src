/*	$NetBSD: cpu.h,v 1.69 2021/04/17 20:12:55 rillig Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)cpu.h	5.4 (Berkeley) 5/9/91
 */

#ifndef _AMD64_CPU_H_
#define _AMD64_CPU_H_

#ifdef __x86_64__

#include <x86/cpu.h>

#ifdef _KERNEL

#if defined(__GNUC__) && !defined(_MODULE)

static struct cpu_info *x86_curcpu(void);
static lwp_t *x86_curlwp(void);

/*
(gdb) disas /sr
Dump of assembler code for function x86_curcpu:
./machine/cpu.h:
58	{
   0xffffffff80913290 <+0>:	55	push   %rbp
   0xffffffff80913291 <+1>:	48 89 e5	mov    %rsp,%rbp
   0xffffffff80913294 <+4>:	50	push   %rax

59		struct cpu_info *ci;
60
61		__asm volatile("movq %%gs:%1, %0" :
=> 0xffffffff80913295 <+5>:	65 48 8b 04 25 88 03 00 00	mov    %gs:0x388,%rax
   0xffffffff8091329e <+14>:	48 89 45 f8	mov    %rax,-0x8(%rbp)

62		    "=r" (ci) :
63		    "m"
64		    (*(struct cpu_info * const *)offsetof(struct cpu_info, ci_self)));
65		return ci;
   0xffffffff809132a2 <+18>:	48 8b 45 f8	mov    -0x8(%rbp),%rax
   0xffffffff809132a6 <+22>:	48 83 c4 08	add    $0x8,%rsp
   0xffffffff809132aa <+26>:	5d	pop    %rbp
   0xffffffff809132ab <+27>:	c3	ret
End of assembler dump.
*/
__inline static struct cpu_info * __unused __nomsan
x86_curcpu(void)
{
	struct cpu_info *ci;

	__asm volatile("movq %%gs:%1, %0" :
	    "=r" (ci) :
	    "m"
	    (*(struct cpu_info * const *)offsetof(struct cpu_info, ci_self)));
	return ci;
}

/*
disas /sr
info r
x/1xg $rsp+8
x/1xg $rsp
https://defuse.ca/online-x86-assembler.htm
  init
   0xffffffff80381290 <+0>:	push   %rbp
    rsp: 0xffffd0805452b958 -> 0xffffd0805452b950
      0xffffd0805452b950:	0xffffd0805452b980 (rbp)
      0xffffd0805452b958:	0xffffffff8038118d (unknown)

   0xffffffff80381291 <+1>:	48 89 e5	mov    %rsp,%rbp
    rbp: 0xffffd0805452b980 -> 0xffffd0805452b950 (rsp)

   0xffffffff80381294 <+4>:	50	push   %rax
    rsp: 0xffffd0805452b950 -> 0xffffd0805452b948
      0xffffd0805452b948:	0x0000000000000000 (rax)

  __asm ...
   0xffffffff80381295 <+5>:	65 48 8b 04 25 80 09 00 00	mov    %gs:0x980,%rax
                                                                  ^^^^^^ ??
    gs: 0 -> 0
    rax: 0 -> 0xffffbb22b09af540 ??? (r14 たまたま？)
      TODO: https://stackoverflow.com/questions/11497563/detail-about-msr-gs-base-in-linux-x86-64

  __asm ... (l = rax)
   0xffffffff8038129e <+14>:	48 89 45 f8	mov    %rax,-0x8(%rbp)

  return l;
   0xffffffff803812a2 <+18>:	48 8b 45 f8	mov    -0x8(%rbp),%rax
   0xffffffff803812a6 <+22>:	48 83 c4 08	add    $0x8,%rsp
   0xffffffff803812aa <+26>:	5d	pop    %rbp
   0xffffffff803812ab <+27>:	c3	ret
End of assembler dump.

---

qemu/target/i386/tcg/translate.c
disas_insn()
    case 0x65:
        s->override = R_GS;
    case 0x40 ... 0x4f: // 48
        if (CODE64(s)) {
            / * REX prefix * /
            prefixes |= PREFIX_REX;
            s->rex_w = (b >> 3) & 1; // true
            s->rex_r = (b & 0x4) << 1; // 0
            s->rex_x = (b & 0x2) << 2; // 0
            s->rex_b = (b & 0x1) << 3; // 0

    // 8b 04
    case 0x8a:
    case 0x8b: / * mov Ev, Gv * /
      modrm: 0x04

   0xffffffff80381295 <+5>:	65 48 8b 04 25 80 09 00 00	mov    %gs:0x980,%rax
                            ^^ GS
                               ^^ REX W
                                  ^^^^^ mov ModR/M 0x04
                                        ^^ ?
                                           ^^^^^^^^^^^ 0x00000980 offsetof(struct cpu_info, ci_curlwp)
*/
__inline static lwp_t * __unused __nomsan __attribute__ ((const))
x86_curlwp(void)
{
	lwp_t *l;

	__asm volatile("movq %%gs:%1, %0" :
	    "=r" (l) :
	    "m"
	    (*(struct cpu_info * const *)offsetof(struct cpu_info, ci_curlwp)));
	return l;
}

#endif	/* __GNUC__ && !_MODULE */

#ifdef XENPV
#define	CLKF_USERMODE(frame)	(curcpu()->ci_xen_clockf_usermode)
#define CLKF_PC(frame)		(curcpu()->ci_xen_clockf_pc)
#else /* XENPV */
#define	CLKF_USERMODE(frame)	USERMODE((frame)->cf_if.if_tf.tf_cs)
#define CLKF_PC(frame)		((frame)->cf_if.if_tf.tf_rip)
#endif /* XENPV */
#define CLKF_INTR(frame)	(curcpu()->ci_idepth > 0)
#define LWP_PC(l)		((l)->l_md.md_regs->tf_rip)

void *cpu_uarea_alloc(bool);
bool cpu_uarea_free(void *);

#endif	/* _KERNEL */

#else	/*	__x86_64__	*/

#include <i386/cpu.h>

#endif	/*	__x86_64__	*/

#endif /* !_AMD64_CPU_H_ */
