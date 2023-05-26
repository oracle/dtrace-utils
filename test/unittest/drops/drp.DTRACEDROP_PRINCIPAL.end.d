/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@nosort */

#pragma D option strsize=2k
#pragma D option bufsize=64k

BEGIN
{
	exit(0);
}

END
{
	trace("00");
	trace("01");
}

END
{
	trace("02");
	trace("03");
}

END
{
	trace("04");
	trace("05");
}

END
{
	trace("06");
	trace("07");
}

END
{
	trace("08");
	trace("09");
}

END
{
	trace("10");
	trace("11");
}

END
{
	trace("12");
	trace("13");
}

END
{
	trace("14");
	trace("15");
}

END
{
	trace("16");
	trace("17");
}

END
{
	trace("18");
	trace("19");
}

END
{
	trace("20");
	trace("21");
}

END
{
	trace("22");
	trace("23");
}

END
{
	trace("24");
	trace("25");
}

END
{
	trace("26");
	trace("27");
}

END
{
	trace("28");
	trace("29");
}

END
{
	trace("30");
	trace("31");
}
