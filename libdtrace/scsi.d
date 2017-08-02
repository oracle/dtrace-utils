/*
 * Oracle Linux DTrace.
 * Copyright © 2009, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma	D depends_on module genunix
#pragma	D depends_on module stmf

/*
 * The scsicmd_t structure should be used by providers
 * to represent a SCSI command block (cdb).
 */
typedef struct scsicmd {
	uint64_t ic_len;	/* CDB length */
	uint8_t  *ic_cdb;	/* CDB data */
} scsicmd_t;

/*
 * Translator for scsicmd_t, translating from a scsi_task_t
 */ 
#pragma D binding "1.5" translator
translator scsicmd_t < scsi_task_t *T > {
	ic_len = T->task_cdb_length;
	ic_cdb = T->task_cdb;
};

/*
 * The xferinfo_t structure can be used by providers to
 * represent transfer information related to a single
 * buffer. The members describing the remote memory
 * are only valid if the transport layer is an RDMA-
 * capable transport like Infiniband
 */
typedef struct xferinfo {
	uintptr_t xfer_laddr;   /* local buffer address */
	uint32_t xfer_loffset;
	uint32_t xfer_lkey;     /* access control to local memory */
	uintptr_t xfer_raddr;   /* remote virtual address */
	uint32_t xfer_roffset;  /* offset from the remote address */
	uint32_t xfer_rkey;     /* access control to remote virtual address */
	uint32_t xfer_len;      /* transfer length */
	uint32_t xfer_type;     /* Read (0) or Write (1) */
} xferinfo_t;

/*
 * the iscsiinfo_t is used to provide identifying information about
 * the target and the initiator and also some PDU level information
 * such as lun, data length and sequence numbers.
 */
typedef struct iscsiinfo {
	string ii_target;	/* target iqn */
	string ii_initiator;	/* initiator iqn */
	string ii_isid;         /* initiator session identifier */
	string ii_tsih;         /* target session identifying handle */
	string ii_transport;    /* transport type ("iser-ib", "sockets") */

	uint64_t ii_lun;	/* target logical unit number */

	uint32_t ii_itt;	/* initiator task tag */
	uint32_t ii_ttt;	/* target transfer tag */

	uint32_t ii_cmdsn;	/* command sequence number */
	uint32_t ii_statsn;	/* status sequence number */
	uint32_t ii_datasn;	/* data sequence number */

	uint32_t ii_datalen;	/* length of data payload */
	uint32_t ii_flags;	/* probe-specific flags */
} iscsiinfo_t;
