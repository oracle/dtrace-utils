/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

/* @@tags: unstable */

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
