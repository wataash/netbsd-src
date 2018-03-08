// http://www.jp.netbsd.org/ja/docs/kernel/pseudo/

/*	$NetBSD: pseudo_dev_skel.h,v 1.1 2007/06/09 11:33:50 dsieger Exp $	*/

/*-
 * Copyright (c) 1998-2006 Brett Lymn (blymn@NetBSD.org)
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

/*
 *
 * Definitions for the Skeleton pseudo device.
 *
 */
// ?
#if 0
#include <sys/param.h>
#include <sys/device.h>
#endif

// maybe should be:
#include <sys/ioccom.h>

#ifndef SKEL_H
#define SKEL_H 1

struct skeleton_params
{
    int number;
    char string[80];
};

/*
 * 332|2222222221111|11111100|00000000
 * 109|8765432109876|54321098|76543210
 * 100|0000001010100|01010011|00000001
 * in  152 (max8191) 'S' 83   0x01
 */

#define SKELTEST_ _IOW('S', 0x1, struct skeleton_params)
#define SKELTEST__ _IOC(IOC_IN, 'S', 0x1, sizeof(skeleton_params))
#define SKELTEST ( \
    (unsigned long)0x80000000 | \
    ((sizeof(struct skeleton_params) & IOCPARM_MASK) << IOCPARM_SHIFT) | \
    ('S' << IOCGROUP_SHIFT) | \
    0x1 \
)

#ifdef _KERNEL

/*
 * Put kernel inter-module interfaces here, this
 * pseudo device has none.
 */

#endif
#endif
