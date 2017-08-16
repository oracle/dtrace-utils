/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

dtrace:::BEGIN
{
	i = 20;
}

profile:::tick-1sec
/i > 0/
{
        i-=2;
        @quant["quantized countdown"]=quantize(i);
        @lquant["linear quantized countdown"]=lquantize(i,0,20,1);
}

profile:::tick-1sec
/i == 0/
{
	trace("blastoff!");
	exit(0);
}
