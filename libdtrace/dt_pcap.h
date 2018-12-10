/*
 * Oracle Linux DTrace.
 * Copyright (c) 2016, 2018 Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PCAP_H
#define	_DT_PCAP_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <dt_list.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

struct dtrace_hdl;

#define	DT_PCAP_DEF_PKTSIZE	1514
#define	DT_PCAPSIZE(sz) \
	((sz > 0 && sz < 65566) ? sz : DT_PCAP_DEF_PKTSIZE)

typedef struct dt_global_pcap {
	dt_list_t dt_pcaps;	/* pcap file info */
	int dt_pcap_pipe[2];	/* both our ends of the pcap tshark pipes */
	pid_t dt_pcap_pid;	/* pid for tshark pipe */
	FILE *dt_pcap_out_fp;	/* stdout for tshark pipe */
	pthread_t dt_pcap_output; /* thread for printing tshark output */
} dt_global_pcap_t;

void dt_pcap_destroy(struct dtrace_hdl *);
const char *dt_pcap_filename(struct dtrace_hdl *, FILE *);
void dt_pcap_dump(struct dtrace_hdl *, const char *, uint64_t, uint64_t,
		  void *, uint32_t, uint32_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PCAP_H */
