/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2011 Oracle Inc.  All rights reserved.
 * Use is subject to license terms.
 */

dtrace:::BEGIN
{
	i = 40;
        t3 = 0;
        t5 = 0;
        t7 = 0;
}

profile:::tick-3hz
/i > 0/
{
        i--;
        t3 += 3;
        @quant["quantized firings"]=quantize(i);
        hits[t3%5]++;
        @lquant["linear quantized firings"]=lquantize(i,0,40,1);
}

profile:::tick-5hz
/i > 0/
{
        i--;
        t5 += 5;
        @quant["quantized firings"]=quantize(i);
        hits[t5%5]++;
        @lquant["linear quantized firings"]=lquantize(i,0,40,1);
}

profile:::tick-7hz
/i > 0/
{
        i--;
        t7 += 7;
        @quant["quantized firings"]=quantize(i);
        hits[t7%5]++;
        @lquant["linear quantized firings"]=lquantize(i,0,40,1);
}

profile:::tick-1sec
/i == 0/
{
	trace("blastoff!");
	exit(0);
}

dtrace:::END
{
    printf ("\n\nHits mod 5: (%i, %i, %i, %i, %i)\n",
             hits[0], hits[1], hits[2], hits[3], hits[4]);
}
