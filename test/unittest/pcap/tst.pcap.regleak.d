/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: pcap() does not leak registers
 *
 * SECTION: Actions and Subroutines/pcap()
 */

/* @@runtest-opts: -e */

BEGIN
{
	/*
	 * If pcap() leaks even a single register, ten invocations will most
	 * certainly result in a compiler error (due to register starvation).
	 */
	pcap(0, PCAP_IP);
	pcap(0, PCAP_IP);
	pcap(0, PCAP_IP);
	pcap(0, PCAP_IP);
	pcap(0, PCAP_IP);
	pcap(0, PCAP_IP);
	pcap(0, PCAP_IP);
	pcap(0, PCAP_IP);
	pcap(0, PCAP_IP);
	pcap(0, PCAP_IP);
	exit(0);
}

ERROR
{
	exit(1);
}
