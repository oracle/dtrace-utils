/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_DIS_H
#define	_DT_DIS_H

#include <dt_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The following disassembler listings can be requested with -xdisasm=....
 *    - After compilation and assembly of a clause function.
 *    - After constructing a probe program.
 *    - After linking in dependencies.
 *    - After all processing, prior to loading the program.
 * The values can be combined to select multiple listings.  The '-S' option
 * must also be supplied in order for disassembler output to be generated.
 */
#define DT_DISASM_OPT_CLAUSE		1	/* default */
#define DT_DISASM_OPT_PROG		2
#define DT_DISASM_OPT_PROG_LINKED	4
#define DT_DISASM_OPT_PROG_FINAL	8

/*
 * Macros to make a call to the disassembler for specific disassembler listings.
 */
#define DT_DISASM_CLAUSE(dtp, cflags, pp, fp) \
	do { \
		if (((cflags) & DTRACE_C_DIFV) && \
		    ((dtp)->dt_disasm & DT_DISASM_OPT_CLAUSE)) \
			dt_dis_program((dtp), (pp), (fp)); \
	} while(0)
#define DT_DISASM_PROG(dtp, cflags, dp, fp, idp, pdp) \
	do { \
		if (((cflags) & DTRACE_C_DIFV) && \
		    ((dtp)->dt_disasm & DT_DISASM_OPT_PROG)) \
			dt_dis_difo((dp), (fp), (idp), (pdp), "program"); \
	} while(0)
#define DT_DISASM_PROG_LINKED(dtp, cflags, dp, fp, idp, pdp) \
	do { \
		if (((cflags) & DTRACE_C_DIFV) && \
		    ((dtp)->dt_disasm & DT_DISASM_OPT_PROG_LINKED)) \
			dt_dis_difo((dp), (fp), (idp), (pdp), \
				    "linked program"); \
	} while(0)
#define DT_DISASM_PROG_FINAL(dtp, cflags, dp, fp, idp, pdp) \
	do { \
		if (((cflags) & DTRACE_C_DIFV) && \
		    ((dtp)->dt_disasm & DT_DISASM_OPT_PROG_FINAL)) \
			dt_dis_difo((dp), (fp), (idp), (pdp), \
				    "final program"); \
	} while(0)

extern void dt_dis_program(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, FILE *fp);
extern void dt_dis_difo(const dtrace_difo_t *dp, FILE *fp,
			const dt_ident_t *idp, const dtrace_probedesc_t *pdp,
			const char *ltype);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_DIS_H */
