/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

io:::start
/execname == "soffice.bin" && args[2]->fi_name == "applicat.rdb"/
{
	@ = lquantize(args[2]->fi_offset != -1 ?
	    args[2]->fi_offset / (1000 * 1024) : -1, 0, 1000);
}
