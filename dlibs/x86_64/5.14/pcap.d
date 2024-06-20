/*
 * Oracle Linux DTrace.
 * Copyright (c) 2016, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D depends_on module vmlinux
#pragma D depends_on provider ip

/*
 * In general, PCAP_<type> names are chosen to match DL_<type> DLPI
 * datalink types.
 */

/*
 * Network-related capture types.  Raw DLT_* values can be used for those
 * not present here; see /usr/include/pcap/bpf.h for the full list.
 */

inline int PCAP_NULL = 0;
#pragma D binding "1.12" PCAP_NULL
inline int PCAP_ETHER = 1;
#pragma D binding "1.12" PCAP_ETHER
inline int PCAP_WIFI = 105;
#pragma D binding "1.12" PCAP_WIFI
inline int PCAP_PPP = 9;
#pragma D binding "1.12" PCAP_PPP
/* PCAP_IP can signify IPv4 or IPv6 header follows. */
inline int PCAP_IP = 12;
#pragma D binding "1.12" PCAP_IP

/* InfiniBand-related capture types. */
inline int PCAP_IPOIB = 242;
#pragma D binding "1.12" PCAP_IPOIB
