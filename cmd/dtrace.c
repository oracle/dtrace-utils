/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/compiler.h>

#include <dtrace.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <alloca.h>
#include <port.h>

#include <dt_git_version.h>

typedef struct dtrace_cmd {
	void (*dc_func)(struct dtrace_cmd *);	/* function to compile arg */
	dtrace_probespec_t dc_spec;		/* probe specifier context */
	char *dc_arg;				/* argument from main argv */
	const char *dc_name;			/* name for error messages */
	const char *dc_desc;			/* desc for error messages */
	dtrace_prog_t *dc_prog;			/* program compiled from arg */
	char dc_ofile[PATH_MAX];		/* derived output file name */
} dtrace_cmd_t;

#define	DMODE_VERS	0	/* display version information and exit (-V) */
#define	DMODE_EXEC	1	/* compile program for enabling (e/E) */
#define	DMODE_LINK	2	/* compile program for linking with ELF (-G) */
#define	DMODE_LIST	3	/* compile program and list probes (-l) */
#define	DMODE_HEADER	4	/* compile program for headergen (-h) */

#define DONE_SAW_END	1	/* If END is not enabled, we pretend */
#define DONE_SAW_EXIT	2	/* exit() action processed */

#define	E_SUCCESS	0
#define	E_ERROR		1
#define	E_USAGE		2

static const char DTRACE_OPTSTR[] =
	"3:6:aAb:Bc:CD:ef:FGhHi:I:lL:m:n:o:p:P:qs:SU:vVwx:X:Z";

static char **g_argv;
static int g_argc;
static char **g_objv;
static int g_objc;
static dtrace_cmd_t *g_cmdv;
static int g_cmdc;
static struct dtrace_proc **g_psv;
static int g_psc;
static int g_pslive;
static char *g_pname;
static int g_quiet;
static int g_testing;
static int g_flowindent;
static int g_intr;
static int g_impatient;
static int g_newline;
static int g_total;
static int g_cflags;
static int g_oflags = 0;
static int g_verbose;
static int g_exec = 1;
static int g_mode = DMODE_EXEC;
static int g_status = E_SUCCESS;
static const char *g_ofile = NULL;
static FILE *g_ofp = NULL;
static dtrace_hdl_t *g_dtp;

static int
usage(FILE *fp)
{
	static const char predact[] = "[[ predicate ] action ]";

	fprintf(fp, "Usage: %s [-32|-64] [-CeFGhHlqSvVwZ] "
	    "[-b bufsz] [-c cmd] [-D name[=def]]\n\t[-I path] [-L path] "
	    "[-o output] [-p pid] [-s script] [-U name]\n\t"
	    "[-x opt[=val]] [-X a|c|s|t]\n\n"
	    "\t[-P provider %s]\n"
	    "\t[-m [ provider: ] module %s]\n"
	    "\t[-f [[ provider: ] module: ] func %s]\n"
	    "\t[-n [[[ provider: ] module: ] func: ] name %s]\n"
	    "\t[-i probe-id %s] [ args ... ]\n\n", g_pname,
	    predact, predact, predact, predact, predact);

	fprintf(fp, "\tpredicate -> '/' D-expression '/'\n");
	fprintf(fp, "\t   action -> '{' D-statements '}'\n");

	fprintf(fp, "\n"
	    "\t-32 generate 32-bit D programs and ELF files\n"
	    "\t-64 generate 64-bit D programs and ELF files\n\n"
	    "\t-b  set trace buffer size\n"
	    "\t-c  run specified command and exit upon its completion\n"
	    "\t-C  run cpp(1) preprocessor on script files\n"
	    "\t-D  define symbol when invoking preprocessor\n"
	    "\t-e  exit after compiling request but prior to enabling probes\n"
	    "\t-f  enable or list probes matching the specified function name\n"
	    "\t-F  coalesce trace output by function\n"
	    "\t-G  generate an ELF file containing embedded dtrace program\n"
	    "\t-h  generate a header file with definitions for static probes\n"
	    "\t-H  print included files when invoking preprocessor\n"
	    "\t-i  enable or list probes matching the specified probe id\n"
	    "\t-I  add include directory to preprocessor search path\n"
	    "\t-l  list probes matching specified criteria\n"
	    "\t-L  add library directory to library search path\n"
	    "\t-m  enable or list probes matching the specified module name\n"
	    "\t-n  enable or list probes matching the specified probe name\n"
	    "\t-o  set output file\n"
	    "\t-p  grab specified process-ID and cache its symbol tables\n"
	    "\t-P  enable or list probes matching the specified provider name\n"
	    "\t-q  set quiet mode (only output explicitly traced data)\n"
	    "\t-s  enable or list probes according to the specified D script\n"
	    "\t-S  print D compiler intermediate code\n"
	    "\t-U  undefine symbol when invoking preprocessor\n"
	    "\t-v  set verbose mode (report stability attributes, arguments)\n"
	    "\t-V  report DTrace API version\n"
	    "\t-w  permit destructive actions\n"
	    "\t-x  enable or modify compiler and tracing options\n"
	    "\t-X  specify ISO C conformance settings for preprocessor\n"
	    "\t-Z  permit probe descriptions that match zero probes\n");

	return E_USAGE;
}

static void
verror(const char *fmt, va_list ap)
{
	int error = errno;

	fprintf(stderr, "%s: ", g_pname);
	vfprintf(stderr, fmt, ap);

	if (fmt[strlen(fmt) - 1] != '\n')
		fprintf(stderr, ": %s\n", strerror(error));
}

/*PRINTFLIKE1*/
_dt_printflike_(1,2)
static void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror(fmt, ap);
	va_end(ap);

	/*
	 * Close the DTrace handle to ensure that any controlled processes are
	 * correctly restored and continued.
	 */
	dtrace_close(g_dtp);

	exit(E_ERROR);
}

/*PRINTFLIKE1*/
_dt_printflike_(1,2)
static void
dfatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	fprintf(stderr, "%s: ", g_pname);
	if (fmt != NULL)
		vfprintf(stderr, fmt, ap);

	va_end(ap);

	if (fmt != NULL && fmt[strlen(fmt) - 1] != '\n')
		fprintf(stderr, ": %s\n",
		    dtrace_errmsg(g_dtp, dtrace_errno(g_dtp)));
	else if (fmt == NULL)
		fprintf(stderr, "%s\n",
		    dtrace_errmsg(g_dtp, dtrace_errno(g_dtp)));

	/*
	 * Close the DTrace handle to ensure that any controlled processes are
	 * correctly restored and continued.
	 */
	dtrace_close(g_dtp);

	exit(E_ERROR);
}

/*PRINTFLIKE1*/
_dt_printflike_(1,2)
static void
error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror(fmt, ap);
	va_end(ap);
}

/*PRINTFLIKE1*/
_dt_printflike_(1,2)
static void
notice(const char *fmt, ...)
{
	va_list ap;

	if (g_quiet)
		return; /* -q or quiet pragma suppresses notice()s */

	va_start(ap, fmt);
	verror(fmt, ap);
	va_end(ap);
}

/*PRINTFLIKE1*/
_dt_printflike_(1,2)
static void
oprintf(const char *fmt, ...)
{
	va_list ap;
	int n;

	if (g_ofp == NULL)
		return;

	va_start(ap, fmt);
	n = vfprintf(g_ofp, fmt, ap);
	va_end(ap);

	if (n < 0) {
		if (errno != EINTR) {
			fatal("failed to write to %s",
			    g_ofile ? g_ofile : "<stdout>");
		}
		clearerr(g_ofp);
	}
}

static char **
make_argv(char *s)
{
	const char *ws = "\f\n\r\t\v ";
	char **argv = malloc(sizeof(char *) * (strlen(s) / 2 + 1));
	int argc = 0;
	char *p = s;

	if (argv == NULL)
		return NULL;

	for (p = strtok(s, ws); p != NULL; p = strtok(NULL, ws))
		argv[argc++] = p;

	if (argc == 0)
		argv[argc++] = s;

	argv[argc] = NULL;
	return argv;
}

static void
print_probe_info(const dtrace_probeinfo_t *p)
{
	char buf[BUFSIZ];
	int i;

	oprintf("\n\tProbe Description Attributes\n");

	oprintf("\t\tIdentifier Names: %s\n",
	    dtrace_stability_name(p->dtp_attr.dtat_name));
	oprintf("\t\tData Semantics:   %s\n",
	    dtrace_stability_name(p->dtp_attr.dtat_data));
	oprintf("\t\tDependency Class: %s\n",
	    dtrace_class_name(p->dtp_attr.dtat_class));

	oprintf("\n\tArgument Attributes\n");

	oprintf("\t\tIdentifier Names: %s\n",
	    dtrace_stability_name(p->dtp_arga.dtat_name));
	oprintf("\t\tData Semantics:   %s\n",
	    dtrace_stability_name(p->dtp_arga.dtat_data));
	oprintf("\t\tDependency Class: %s\n",
	    dtrace_class_name(p->dtp_arga.dtat_class));

	oprintf("\n\tArgument Types\n");

	for (i = 0; i < p->dtp_argc; i++) {
		if (ctf_type_name(p->dtp_argv[i].dtt_ctfp,
		    p->dtp_argv[i].dtt_type, buf, sizeof(buf)) == NULL)
			strlcpy(buf, "(unknown)", sizeof(buf));
		oprintf("\t\targs[%d]: %s\n", i, buf);
	}

	if (p->dtp_argc == 0)
		oprintf("\t\tNone\n");

	oprintf("\n");
}

/*ARGSUSED*/
static int
info_stmt(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_stmtdesc_t *stp, dtrace_ecbdesc_t **last)
{
	dtrace_ecbdesc_t *edp = stp->dtsd_ecbdesc;
	dtrace_probedesc_t *pdp = &edp->dted_probe;
	dtrace_probeinfo_t p;

	if (edp == *last)
		return 0;

	oprintf("\n%s:%s:%s:%s\n", pdp->prv, pdp->mod, pdp->fun, pdp->prb);

	if (dtrace_probe_info(dtp, pdp, &p) == 0)
		print_probe_info(&p);
	else
		return -1;

	*last = edp;
	return 0;
}

/*
 * Execute the specified program by enabling the corresponding instrumentation.
 * If -e has been specified, we get the program info but do not enable it.  If
 * -v has been specified, we print a stability report for the program.
 */
static void
exec_prog(const dtrace_cmd_t *dcp)
{
	dtrace_ecbdesc_t *last = NULL;
	dtrace_proginfo_t dpi;

	if (!g_exec) {
		dtrace_program_info(g_dtp, dcp->dc_prog, &dpi);
	} else if (dtrace_program_exec(g_dtp, dcp->dc_prog, &dpi) == -1) {
		dfatal("failed to enable '%s'", dcp->dc_name);
	} else {
		notice("%s '%s' matched %u probe%s\n",
		    dcp->dc_desc, dcp->dc_name,
		    dpi.dpi_matches, dpi.dpi_matches == 1 ? "" : "s");
	}

	if (g_verbose) {
		oprintf("\nStability attributes for %s %s:\n",
		    dcp->dc_desc, dcp->dc_name);

		oprintf("\n\tMinimum Probe Description Attributes\n");
		oprintf("\t\tIdentifier Names: %s\n",
		    dtrace_stability_name(dpi.dpi_descattr.dtat_name));
		oprintf("\t\tData Semantics:   %s\n",
		    dtrace_stability_name(dpi.dpi_descattr.dtat_data));
		oprintf("\t\tDependency Class: %s\n",
		    dtrace_class_name(dpi.dpi_descattr.dtat_class));

		oprintf("\n\tMinimum Statement Attributes\n");

		oprintf("\t\tIdentifier Names: %s\n",
		    dtrace_stability_name(dpi.dpi_stmtattr.dtat_name));
		oprintf("\t\tData Semantics:   %s\n",
		    dtrace_stability_name(dpi.dpi_stmtattr.dtat_data));
		oprintf("\t\tDependency Class: %s\n",
		    dtrace_class_name(dpi.dpi_stmtattr.dtat_class));

		if (!g_exec)
			dtrace_stmt_iter(g_dtp, dcp->dc_prog,
			    (dtrace_stmt_f *)info_stmt, &last);
		else
			oprintf("\n");
	}

	g_total += dpi.dpi_matches;
}

/*
 * Link the specified D program in DOF form into an ELF file for use in either
 * helpers, userland provider definitions, or both.  If -o was specified, that
 * path is used as the output file name.  If -o wasn't specified and the input
 * program is from a script whose name is %.d, use basename(%.o) as the output
 * file name.  Otherwise we use "d.out" as the default output file name.
 */
static void
link_prog(dtrace_cmd_t *dcp)
{
	char *p;

	if (g_cmdc == 1 && g_ofile != NULL) {
		strlcpy(dcp->dc_ofile, g_ofile, sizeof(dcp->dc_ofile));
	} else if ((p = strrchr(dcp->dc_arg, '.')) != NULL &&
	    strcmp(p, ".d") == 0) {
		p[0] = '\0'; /* strip .d suffix */
		snprintf(dcp->dc_ofile, sizeof(dcp->dc_ofile),
		    "%s.o", basename(dcp->dc_arg));
	} else
		snprintf(dcp->dc_ofile, sizeof(dcp->dc_ofile),
		    g_cmdc > 1 ?  "%s.%d" : "%s", "d.out", (int)(dcp - g_cmdv));

	if (dtrace_program_link(g_dtp, dcp->dc_prog, DTRACE_D_PROBES,
	    dcp->dc_ofile, g_objc, g_objv) != 0)
		dfatal("failed to link %s %s", dcp->dc_desc, dcp->dc_name);
}

/*ARGSUSED*/
static int
list_probe(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp, void *arg)
{
	dtrace_probeinfo_t p;

	oprintf("%5d %10s %17s %33s %s\n",
		pdp->id, pdp->prv, pdp->mod, pdp->fun, pdp->prb);

	if (g_verbose) {
		if (dtrace_probe_info(dtp, pdp, &p) == 0)
			print_probe_info(&p);
		else
			return -1;
	}

	return 0;
}

/*ARGSUSED*/
static int
list_stmt(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_stmtdesc_t *stp, dtrace_ecbdesc_t **last)
{
	dtrace_ecbdesc_t *edp = stp->dtsd_ecbdesc;

	if (edp == *last)
		return 0;

	if (dtrace_probe_iter(g_dtp, &edp->dted_probe, list_probe, NULL) < 0) {
		error("failed to match %s:%s:%s:%s: %s\n",
		    edp->dted_probe.prv, edp->dted_probe.mod,
		    edp->dted_probe.fun, edp->dted_probe.prb,
		    dtrace_errmsg(dtp, dtrace_errno(dtp)));
	}

	*last = edp;
	return 0;
}

/*
 * List the probes corresponding to the specified program by iterating over
 * each statement and then matching probes to the statement probe descriptions.
 */
static void
list_prog(const dtrace_cmd_t *dcp)
{
	dtrace_ecbdesc_t *last = NULL;

	dtrace_stmt_iter(g_dtp, dcp->dc_prog,
	    (dtrace_stmt_f *)list_stmt, &last);
}

static void
compile_file(dtrace_cmd_t *dcp)
{
	char *arg0;
	FILE *fp;

	if ((fp = fopen(dcp->dc_arg, "r")) == NULL)
		fatal("failed to open %s", dcp->dc_arg);

	arg0 = g_argv[0];
	g_argv[0] = dcp->dc_arg;

	if ((dcp->dc_prog = dtrace_program_fcompile(g_dtp, fp,
	    g_cflags, g_argc, g_argv)) == NULL)
		dfatal("failed to compile script %s", dcp->dc_arg);

	g_argv[0] = arg0;
	fclose(fp);

	dcp->dc_desc = "script";
	dcp->dc_name = dcp->dc_arg;
}

static void
compile_str(dtrace_cmd_t *dcp)
{
	char *p;

	if ((dcp->dc_prog = dtrace_program_strcompile(g_dtp, dcp->dc_arg,
	    dcp->dc_spec, g_cflags | DTRACE_C_PSPEC, g_argc, g_argv)) == NULL)
		dfatal("invalid probe specifier %s", dcp->dc_arg);

	if ((p = strpbrk(dcp->dc_arg, "{/;")) != NULL)
		*p = '\0'; /* crop name for reporting */

	dcp->dc_desc = "description";
	dcp->dc_name = dcp->dc_arg;
}

static void
prochandler(pid_t pid, const char *msg, void *arg)
{
	/*
	 * These days, this is only called on process death.  We can easily
	 * prove this by checking P's nullity state.
	 */

	if (pid < 0) {
		g_pslive--;
		return;
	}

	/*
	 * We don't know what's happened here, but at least we can print a
	 * message out, if there is one.
	 */
	if (msg != NULL)
		notice("%s\n", msg);
}

/*ARGSUSED*/
static int
errhandler(const dtrace_errdata_t *data, void *arg)
{
	error("%s\n", data->dteda_msg);
	return DTRACE_HANDLE_OK;
}

/*ARGSUSED*/
static int
drophandler(const dtrace_dropdata_t *data, void *arg)
{
	error("%s\n", data->dtdda_msg);
	return DTRACE_HANDLE_OK;
}

/*ARGSUSED*/
static int
setopthandler(const dtrace_setoptdata_t *data, void *arg)
{
	if (strcmp(data->dtsda_option, "quiet") == 0)
		g_quiet = data->dtsda_newval != DTRACEOPT_UNSET;

	if (strcmp(data->dtsda_option, "flowindent") == 0)
		g_flowindent = data->dtsda_newval != DTRACEOPT_UNSET;

	return DTRACE_HANDLE_OK;
}

#define	BUFDUMPHDR(hdr) \
	printf("%s: %s%s\n", g_pname, hdr, strlen(hdr) > 0 ? ":" : "");

#define	BUFDUMPSTR(ptr, field) \
	printf("%s: %20s => ", g_pname, #field);	\
	if ((ptr)->field != NULL) {			\
		const char *c = (ptr)->field;		\
		printf("\"");				\
		do {					\
			if (*c == '\n') {		\
				printf("\\n");		\
				continue;		\
			}				\
							\
			printf("%c", *c);		\
		} while (*c++ != '\0');			\
		printf("\"\n");				\
	} else						\
		printf("<NULL>\n");

#define	BUFDUMPASSTR(ptr, field, str) \
	printf("%s: %20s => %s\n", g_pname, #field, str);

#define	BUFDUMP(ptr, field) \
	printf("%s: %20s => %lld\n", g_pname, #field, \
	    (long long)(ptr)->field);

#define	BUFDUMPPTR(ptr, field) \
	printf("%s: %20s => %s\n", g_pname, #field, \
	    (ptr)->field != NULL ? "<non-NULL>" : "<NULL>");

/*ARGSUSED*/
static int
bufhandler(const dtrace_bufdata_t *bufdata, void *arg)
{
	const dtrace_aggdata_t *agg = bufdata->dtbda_aggdata;
	const dtrace_recdesc_t *rec = bufdata->dtbda_recdesc;
	const dtrace_probedesc_t *pd;
	uint32_t flags = bufdata->dtbda_flags;
	char buf[512], *c = buf, *end = c + sizeof(buf);
	int i, printed;

	struct {
		const char *name;
		uint32_t value;
	} flagnames[] = {
	    { "AGGVAL",		DTRACE_BUFDATA_AGGVAL },
	    { "AGGKEY",		DTRACE_BUFDATA_AGGKEY },
	    { "AGGFORMAT",	DTRACE_BUFDATA_AGGFORMAT },
	    { "AGGLAST",	DTRACE_BUFDATA_AGGLAST },
	    { "???",		UINT32_MAX },
	    { NULL }
	};

	if (bufdata->dtbda_probe != NULL)
		pd = bufdata->dtbda_probe->dtpda_pdesc;
	else
		pd = NULL;

	BUFDUMPHDR(">>> Called buffer handler");
	BUFDUMPHDR("");

	BUFDUMPHDR("  dtrace_bufdata");
	BUFDUMPSTR(bufdata, dtbda_buffered);
	BUFDUMPPTR(bufdata, dtbda_probe);
	BUFDUMPPTR(bufdata, dtbda_aggdata);
	BUFDUMPPTR(bufdata, dtbda_recdesc);

	snprintf(c, end - c, "0x%x ", bufdata->dtbda_flags);
	c += strlen(c);

	for (i = 0, printed = 0; flagnames[i].name != NULL; i++) {
		if (!(flags & flagnames[i].value))
			continue;

		snprintf(c, end - c,
		    "%s%s", printed++ ? " | " : "(", flagnames[i].name);
		c += strlen(c);
		flags &= ~flagnames[i].value;
	}

	if (printed)
		snprintf(c, end - c, ")");

	BUFDUMPASSTR(bufdata, dtbda_flags, buf);
	BUFDUMPHDR("");

	if (pd != NULL) {
		BUFDUMPHDR("  dtrace_probedesc");
		BUFDUMPSTR(pd, prv);
		BUFDUMPSTR(pd, mod);
		BUFDUMPSTR(pd, fun);
		BUFDUMPSTR(pd, prb);
		BUFDUMPHDR("");
	}

	if (rec != NULL) {
		BUFDUMPHDR("  dtrace_recdesc");
		BUFDUMP(rec, dtrd_action);
		BUFDUMP(rec, dtrd_size);

		if (agg != NULL) {
			uint8_t *data;
			int lim = rec->dtrd_size;

			sprintf(buf, "%d (data: ", rec->dtrd_offset);
			c = buf + strlen(buf);

			if (lim > sizeof(uint64_t))
				lim = sizeof(uint64_t);

			data = (uint8_t *)agg->dtada_data + rec->dtrd_offset;

			for (i = 0; i < lim; i++) {
				snprintf(c, end - c, "%s%02x",
				    i == 0 ? "" : " ", *data++);
				c += strlen(c);
			}

			snprintf(c, end - c,
			    "%s)", lim < rec->dtrd_size ? " ..." : "");
			BUFDUMPASSTR(rec, dtrd_offset, buf);
		} else {
			BUFDUMP(rec, dtrd_offset);
		}

		BUFDUMPHDR("");
	}

	if (agg != NULL) {
		dtrace_aggdesc_t *desc = agg->dtada_desc;

		BUFDUMPHDR("  dtrace_aggdesc");
		BUFDUMPSTR(desc, dtagd_name);
		BUFDUMP(desc, dtagd_varid);
		BUFDUMP(desc, dtagd_nkrecs);	/* FIXME: +1 for varid */
		BUFDUMPHDR("");
	}

	return DTRACE_HANDLE_OK;
}

/*ARGSUSED*/
static int
chewrec(const dtrace_probedata_t *data, const dtrace_recdesc_t *rec, void *arg)
{
	dtrace_actkind_t act;
	uintptr_t addr;

	if (rec == NULL) {
		/*
		 * We have processed the final record; output the newline if
		 * we're not in quiet mode.
		 */
		if (!g_quiet)
			oprintf("\n");

		return DTRACE_CONSUME_NEXT;
	}

	act = rec->dtrd_action;
	addr = (uintptr_t)data->dtpda_data;

	if (act == DTRACEACT_EXIT) {
		g_status = *((uint32_t *)addr);
		return DTRACE_CONSUME_NEXT;
	}

	return DTRACE_CONSUME_THIS;
}

/*ARGSUSED*/
static int
chew(const dtrace_probedata_t *data, void *arg)
{
	dtrace_probedesc_t *pd = data->dtpda_pdesc;
	processorid_t cpu = data->dtpda_cpu;
	static int heading;

	if (g_impatient) {
		g_newline = 0;
		return DTRACE_CONSUME_ABORT;
	}

	if (heading == 0) {
		if (!g_flowindent) {
			if (!g_quiet) {
				if (!g_testing) {
					oprintf("%3s %6s %32s\n",
					    "CPU", "ID", "FUNCTION:NAME");
				} else {
					oprintf("%32s\n", "FUNCTION:NAME");
				}
			}
		} else {
			if (!g_testing) {
				oprintf("%3s %-41s\n", "CPU", "FUNCTION");
			} else {
				oprintf("%-41s\n", "FUNCTION");
			}
		}
		heading = 1;
	}

	if (!g_flowindent) {
		if (!g_quiet) {
			size_t len = strlen(pd->fun) + strlen(pd->prb) + 2;
			char *name = alloca(len);

			if (name == NULL)
				fatal("failed to allocate memory for name");

			snprintf(name, len, "%s:%s", pd->fun, pd->prb);

			if (!g_testing)
				oprintf("%3d %6d %32s ", cpu, pd->id, name);
			else
				oprintf("%32s ", name);
		}
	} else {
		int indent = data->dtpda_indent;
		char *name;
		size_t len;

		if (data->dtpda_flow == DTRACEFLOW_NONE) {
			len = indent + strlen(pd->fun) + strlen(pd->prb) + 5;
			name = alloca(len);
			snprintf(name, len, "%*s%s%s:%s", indent, "",
				 data->dtpda_prefix, pd->fun, pd->prb);
		} else {
			len = indent + strlen(pd->fun) + 5;
			name = alloca(len);
			snprintf(name, len, "%*s%s%s", indent, "",
				 data->dtpda_prefix, pd->fun);
		}

		if (!g_testing) {
			oprintf("%3d %-41s ", cpu, name);
		} else {
			oprintf("%-41s ", name);
		}
	}

	return DTRACE_CONSUME_THIS;
}

static void
go(void)
{
	int i;
	dtrace_optval_t quiet;

	struct {
		char *name;
		char *optname;
		dtrace_optval_t val;
	} bufs[] = {
		{ "buffer size", "bufsize" },
		{ "aggregation size", "aggsize" },
		{ "speculation size", "specsize" },
		{ "dynamic variable size", "dynvarsize" },
		{ NULL }
	}, rates[] = {
		{ "cleaning rate", "cleanrate" },
		{ "status rate", "statusrate" },
		{ NULL }
	};

	for (i = 0; bufs[i].name != NULL; i++) {
		if (dtrace_getopt(g_dtp, bufs[i].optname, &bufs[i].val) == -1)
			fatal("couldn't get option %s", bufs[i].optname);
	}

	for (i = 0; rates[i].name != NULL; i++) {
		if (dtrace_getopt(g_dtp, rates[i].optname, &rates[i].val) == -1)
			fatal("couldn't get option %s", rates[i].optname);
	}

	if (dtrace_go(g_dtp, g_cflags) == -1)
		dfatal("could not enable tracing");

	dtrace_getopt(g_dtp, "quietresize", &quiet);

	if (quiet != DTRACEOPT_UNSET)
		return;

	for (i = 0; bufs[i].name != NULL; i++) {
		dtrace_optval_t j = 0, mul = 10;
		dtrace_optval_t nsize;

		if (bufs[i].val == DTRACEOPT_UNSET)
			continue;

		dtrace_getopt(g_dtp, bufs[i].optname, &nsize);

		if (nsize == DTRACEOPT_UNSET || nsize == 0)
			continue;

		if (nsize >= bufs[i].val - sizeof(uint64_t))
			continue;

		for (; (INT64_C(1) << mul) <= nsize; j++, mul += 10)
			continue;

		if (!(nsize & ((INT64_C(1) << (mul - 10)) - 1))) {
			error("%s lowered to %lld%c\n", bufs[i].name,
			    (long long)nsize >> (mul - 10), " kmgtpe"[j]);
		} else {
			error("%s lowered to %lld bytes\n", bufs[i].name,
			    (long long)nsize);
		}
	}

	for (i = 0; rates[i].name != NULL; i++) {
		dtrace_optval_t nval;
		char *dir;

		if (rates[i].val == DTRACEOPT_UNSET)
			continue;

		dtrace_getopt(g_dtp, rates[i].optname, &nval);

		if (nval == DTRACEOPT_UNSET || nval == 0)
			continue;

		if (rates[i].val == nval)
			continue;

		dir = nval > rates[i].val ? "reduced" : "increased";

		if (nval <= NANOSEC && (NANOSEC % nval) == 0) {
			error("%s %s to %lld hz\n", rates[i].name, dir,
			    (long long)NANOSEC / (long long)nval);
			continue;
		}

		if ((nval % NANOSEC) == 0) {
			error("%s %s to once every %lld seconds\n",
			    rates[i].name, dir,
			    (long long)nval / (long long)NANOSEC);
			continue;
		}

		error("%s %s to once every %lld nanoseconds\n",
		    rates[i].name, dir, (long long)nval);
	}
}

/*ARGSUSED*/
static void
intr(int signo)
{
	if (!g_intr)
		g_newline = 1;

	if (g_intr++)
		g_impatient = 1;
}

int
main(int argc, char *argv[])
{
	dtrace_bufdesc_t buf;
	struct sigaction act, oact;
	dtrace_status_t status[2];
	dtrace_optval_t opt;
	dtrace_cmd_t *dcp;

	int done = 0, mode = 0;
	int err, i, c;
	char *p, **v;
	pid_t pid;
	struct dtrace_proc *proc;

	g_ofp = stdout;

	g_pname = basename(argv[0]);

	if (argc == 1)
		return usage(stderr);

	if ((g_argv = malloc(sizeof(char *) * argc)) == NULL ||
	    (g_cmdv = malloc(sizeof(dtrace_cmd_t) * argc)) == NULL ||
	    (g_psv = malloc(sizeof(struct dtrace_proc *) * argc)) == NULL)
		fatal("failed to allocate memory for arguments");

	g_argv[g_argc++] = argv[0];	/* propagate argv[0] to D as $0/$$0 */
	argv[0] = g_pname;		/* rewrite argv[0] for getopt errors */

	memset(status, 0, sizeof(status));
	memset(&buf, 0, sizeof(buf));

	/*
	 * Make an initial pass through argv[] processing any arguments that
	 * affect our behavior mode (g_mode) and flags used for dtrace_open().
	 * We also accumulate arguments that are not affiliated with getopt
	 * options into g_argv[], and abort if any invalid options are found.
	 */
	optind = 1;

	while ((c = getopt(argc, argv, DTRACE_OPTSTR)) != EOF) {
		switch (c) {
		case '3':
			if (strcmp(optarg, "2") != 0) {
				fprintf(stderr,
				    "%s: illegal option -- 3%s\n",
				    argv[0], optarg);
				return usage(stderr);
			}
			g_oflags &= ~DTRACE_O_LP64;
			g_oflags |= DTRACE_O_ILP32;
			break;

		case '6':
			if (strcmp(optarg, "4") != 0) {
				fprintf(stderr,
				    "%s: illegal option -- 6%s\n",
				    argv[0], optarg);
				return usage(stderr);
			}
			g_oflags &= ~DTRACE_O_ILP32;
			g_oflags |= DTRACE_O_LP64;
			break;

		case 'e':
			g_exec = 0;
			done = 1;
			break;

		case 'h':
			g_mode = DMODE_HEADER;
			g_cflags |= DTRACE_C_ZDEFS; /* -h implies -Z */
			g_exec = 0;
			mode++;
			break;

		case 'G':
			g_mode = DMODE_LINK;
			g_cflags |= DTRACE_C_ZDEFS; /* -G implies -Z */
			g_exec = 0;
			mode++;
			break;

		case 'l':
			g_mode = DMODE_LIST;
			g_cflags |= DTRACE_C_ZDEFS; /* -l implies -Z */
			mode++;
			break;

		case 'v':
			g_verbose++;
			break;

		case 'V':
			g_mode = DMODE_VERS;
			mode++;
			break;

		default:
			if (strchr(DTRACE_OPTSTR, c) == NULL)
				return usage(stderr);
		}
	}

	for (; optind < argc; optind++)
		g_argv[g_argc++] = argv[optind];

	if (mode > 1) {
		fprintf(stderr, "%s: only one of the [-GhlV] options "
		    "can be specified at a time\n", g_pname);
		return E_USAGE;
	}

	if (g_mode == DMODE_VERS) {
		if (!g_verbose)
			return printf("%s: %s\n", g_pname, _dtrace_version) <= 0;
		else {
			printf("%s: %s\n", g_pname, _dtrace_version);
			printf("This is DTrace %s\n", _DT_VERSION);
			printf("dtrace(1) version-control ID: %s\n", DT_GIT_VERSION);
			return printf("libdtrace version-control ID: %s\n", _libdtrace_vcs_version) <= 0;
		}
	}

	/*
	 * If we're in linker mode and the data model hasn't been specified,
	 * we try to guess the appropriate setting by examining the object
	 * files. We ignore certain errors since we'll catch them later when
	 * we actually process the object files.
	 */
	if (g_mode == DMODE_LINK &&
	    (g_oflags & (DTRACE_O_ILP32 | DTRACE_O_LP64)) == 0 &&
	    elf_version(EV_CURRENT) != EV_NONE) {
		int fd;
		Elf *elf;
		GElf_Ehdr ehdr;

		for (i = 1; i < g_argc; i++) {
			if ((fd = open(g_argv[i], O_RDONLY)) == -1)
				break;

			if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
				close(fd);
				break;
			}

			if (elf_kind(elf) != ELF_K_ELF ||
			    gelf_getehdr(elf, &ehdr) == NULL) {
				close(fd);
				elf_end(elf);
				break;
			}

			close(fd);
			elf_end(elf);

			if (ehdr.e_ident[EI_CLASS] == ELFCLASS64) {
				if (g_oflags & DTRACE_O_ILP32) {
					fatal("can't mix 32-bit and 64-bit "
					    "object files\n");
				}
				g_oflags |= DTRACE_O_LP64;
			} else if (ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
				if (g_oflags & DTRACE_O_LP64) {
					fatal("can't mix 32-bit and 64-bit "
					    "object files\n");
				}
				g_oflags |= DTRACE_O_ILP32;
			} else {
				break;
			}
		}
	}

	/*
	 * Open libdtrace.
	 */
	g_dtp = dtrace_open(DTRACE_VERSION, g_oflags, &err);
	if (g_dtp == NULL)
		fatal("failed to initialize dtrace: %s\n",
		      dtrace_errmsg(NULL, err));

	/*
	 * Set default options.
	 */
	dtrace_setopt(g_dtp, "bufsize", "4m");
	dtrace_setopt(g_dtp, "aggsize", "4m");

	/*
	 * The very first thing we do after buffer-size sanitization is run
	 * through the environment and set DTrace options based on environment
	 * variables.
	 */
	dtrace_setoptenv(g_dtp, "DTRACE_OPT_");

	/*
	 * If -G is specified, enable -xlink=dynamic and -xunodefs to permit
	 * references to undefined symbols to remain as unresolved relocations.
	 */
	if (g_mode == DMODE_LINK) {
		dtrace_setopt(g_dtp, "linkmode", "dynamic");
		dtrace_setopt(g_dtp, "unodefs", NULL);

		/*
		 * Use the remaining arguments as the list of object files
		 * when in linker mode.
		 */
		g_objc = g_argc - 1;
		g_objv = g_argv + 1;

		/*
		 * We still use g_argv[0], the name of the executable.
		 */
		g_argc = 1;
	}

	/*
	 * Now that we have libdtrace open, make a second pass through argv[]
	 * to perform any dtrace_setopt() calls and change any compiler flags.
	 * We also accumulate any program specifications into our g_cmdv[] at
	 * this time; these will compiled as part of the fourth processing pass.
	 */
	optind = 1;
	while ((c = getopt(argc, argv, DTRACE_OPTSTR)) != EOF) {
		switch (c) {
		case 'b':
			if (dtrace_setopt(g_dtp,
				"bufsize", optarg) != 0)
				dfatal("failed to set -b %s", optarg);
			break;

		case 'B':
			g_ofp = NULL;
			break;

		case 'C':
			g_cflags |= DTRACE_C_CPP;
			break;

		case 'D':
			if (dtrace_setopt(g_dtp, "define", optarg) != 0)
				dfatal("failed to set -D %s", optarg);
			break;

		case 'f':
			dcp = &g_cmdv[g_cmdc++];
			dcp->dc_func = compile_str;
			dcp->dc_spec = DTRACE_PROBESPEC_FUNC;
			dcp->dc_arg = optarg;
			break;

		case 'F':
			if (dtrace_setopt(g_dtp, "flowindent", 0) != 0)
				dfatal("failed to set -F");
			break;

		case 'H':
			if (dtrace_setopt(g_dtp, "cpphdrs", 0) != 0)
				dfatal("failed to set -H");
			break;

		case 'i':
			dcp = &g_cmdv[g_cmdc++];
			dcp->dc_func = compile_str;
			dcp->dc_spec = DTRACE_PROBESPEC_NAME;
			dcp->dc_arg = optarg;
			break;

		case 'I':
			if (dtrace_setopt(g_dtp, "incdir", optarg) != 0)
				dfatal("failed to set -I %s", optarg);
			break;

		case 'L':
			if (dtrace_setopt(g_dtp, "libdir", optarg) != 0)
				dfatal("failed to set -L %s", optarg);
			break;

		case 'm':
			dcp = &g_cmdv[g_cmdc++];
			dcp->dc_func = compile_str;
			dcp->dc_spec = DTRACE_PROBESPEC_MOD;
			dcp->dc_arg = optarg;
			break;

		case 'n':
			dcp = &g_cmdv[g_cmdc++];
			dcp->dc_func = compile_str;
			dcp->dc_spec = DTRACE_PROBESPEC_NAME;
			dcp->dc_arg = optarg;
			break;

		case 'P':
			dcp = &g_cmdv[g_cmdc++];
			dcp->dc_func = compile_str;
			dcp->dc_spec = DTRACE_PROBESPEC_PROVIDER;
			dcp->dc_arg = optarg;
			break;

		case 'q':
			if (dtrace_setopt(g_dtp, "quiet", 0) != 0)
				dfatal("failed to set -q");
			break;

		case 'o':
			g_ofile = optarg;
			break;

		case 's':
			dcp = &g_cmdv[g_cmdc++];
			dcp->dc_func = compile_file;
			dcp->dc_spec = DTRACE_PROBESPEC_NONE;
			dcp->dc_arg = optarg;
			break;

		case 'S':
			g_cflags |= DTRACE_C_DIFV;
			break;

		case 'U':
			if (dtrace_setopt(g_dtp, "undef", optarg) != 0)
				dfatal("failed to set -U %s", optarg);
			break;

		case 'w':
			if (dtrace_setopt(g_dtp, "destructive", 0) != 0)
				dfatal("failed to set -w");
			break;

		case 'x':
			if ((p = strchr(optarg, '=')) != NULL)
				*p++ = '\0';

			if (dtrace_setopt(g_dtp, optarg, p) != 0)
				dfatal("failed to set -x %s", optarg);
			break;

		case 'X':
			if (dtrace_setopt(g_dtp, "stdc", optarg) != 0)
				dfatal("failed to set -X %s", optarg);
			break;

		case 'Z':
			g_cflags |= DTRACE_C_ZDEFS;
			break;

		default:
			if (strchr(DTRACE_OPTSTR, c) == NULL)
				return usage(stderr);
		}
	}

	if (g_ofp == NULL && g_mode != DMODE_EXEC) {
		fprintf(stderr, "%s: -B not valid in combination"
		    " with [-Gl] options\n", g_pname);
		return E_USAGE;
	}

	if (g_ofp == NULL && g_ofile != NULL) {
		fprintf(stderr, "%s: -B not valid in combination"
		    " with -o option\n", g_pname);
		return E_USAGE;
	}

	/*
	 * Turn on testing mode if requested.  This only affects dtrace.c, so is
	 * not controlled by a dtrace option.  This quiesces a variety of
	 * nondetermistic output, for better expected-results comparison.
	 */
	if (getenv("_DTRACE_TESTING") != NULL) {
		g_testing = 1;
		if (dtrace_setopt(g_dtp, "quietresize", 0) != 0)
			fprintf(stderr, "%s: cannot set quietresize for "
			    "testing", g_pname);
	}

	/*
	 * In our third pass we handle any command-line options related to
	 * grabbing or creating victim processes.  The behavior of these calls
	 * may been affected by any library options set by the second pass.
	 */
	optind = 1;
	while ((c = getopt(argc, argv, DTRACE_OPTSTR)) != EOF) {
		switch (c) {
		case 'c':
			if ((v = make_argv(optarg)) == NULL)
				fatal("failed to allocate memory");

			proc = dtrace_proc_create(g_dtp, v[0], v, 0);
			if (proc == NULL) {
				free(v);
				dfatal(NULL); /* dtrace_errmsg() only */
			}

			g_psv[g_psc++] = proc;
			free(v);
			break;

		case 'p':
			errno = 0;
			pid = strtol(optarg, &p, 10);

			if (errno != 0 || p == optarg || p[0] != '\0')
				fatal("invalid pid: %s\n", optarg);

			proc = dtrace_proc_grab_pid(g_dtp, pid, 0);
			if (proc == NULL)
				dfatal(NULL); /* dtrace_errmsg() only */

			g_psv[g_psc++] = proc;
			break;
		}
	}

	/*
	 * Now we are ready to initialize libdtrace based on the chosen
	 * options.
	 */
	if (dtrace_init(g_dtp) < 0)
		dfatal("failed to initialize libdtrace");

	/*
	 * In our fourth pass we finish g_cmdv[] by calling dc_func to convert
	 * each string or file specification into a compiled program structure.
	 */
	for (i = 0; i < g_cmdc; i++)
		g_cmdv[i].dc_func(&g_cmdv[i]);

	/*
	 * We only need to register handler if we are going to execute probe
	 * clauses.
	 */
	if (g_mode == DMODE_EXEC) {
		if (dtrace_handle_err(g_dtp, &errhandler, NULL) == -1)
			dfatal("failed to establish error handler");

		if (dtrace_handle_drop(g_dtp, &drophandler, NULL) == -1)
			dfatal("failed to establish drop handler");

		if (dtrace_handle_proc(g_dtp, &prochandler, NULL) == -1)
			dfatal("failed to establish proc handler");

		if (dtrace_handle_setopt(g_dtp, &setopthandler, NULL) == -1)
			dfatal("failed to establish setopt handler");

		if (g_ofp == NULL &&
		    dtrace_handle_buffered(g_dtp, &bufhandler, NULL) == -1)
			dfatal("failed to establish buffered handler");
	}

	dtrace_getopt(g_dtp, "flowindent", &opt);
	g_flowindent = opt != DTRACEOPT_UNSET;

	dtrace_getopt(g_dtp, "quiet", &opt);
	g_quiet = opt != DTRACEOPT_UNSET;

	/*
	 * Now make a fifth and final pass over the options that have been
	 * turned into programs and saved in g_cmdv[], performing any mode-
	 * specific processing.  If g_mode is DMODE_EXEC, we will break out
	 * of the switch() and continue on to the data processing loop.  For
	 * other modes, we will exit dtrace once mode-specific work is done.
	 */
	switch (g_mode) {
	case DMODE_EXEC:
		if (g_ofile != NULL && (g_ofp = fopen(g_ofile, "a")) == NULL)
			fatal("failed to open output file '%s'", g_ofile);

		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = intr;
		if (sigaction(SIGINT, NULL, &oact) == 0 && oact.sa_handler != SIG_IGN)
			sigaction(SIGINT, &act, NULL);
		if (sigaction(SIGTERM, NULL, &oact) == 0 && oact.sa_handler != SIG_IGN)
			sigaction(SIGTERM, &act, NULL);

		for (i = 0; i < g_cmdc; i++)
			exec_prog(&g_cmdv[i]);

		if (done || g_intr) {
			dtrace_close(g_dtp);
			return g_status;
		}
		break;

	case DMODE_LINK:
		if (g_cmdc == 0) {
			fprintf(stderr, "%s: -G requires one or more "
			    "scripts or enabling options\n", g_pname);
			dtrace_close(g_dtp);
			return E_USAGE;
		}

		for (i = 0; i < g_cmdc; i++)
			link_prog(&g_cmdv[i]);

		if (g_cmdc > 1 && g_ofile != NULL) {
			char **objv = alloca(g_cmdc * sizeof(char *));

			for (i = 0; i < g_cmdc; i++)
				objv[i] = g_cmdv[i].dc_ofile;

			if (dtrace_program_link(g_dtp, NULL, DTRACE_D_PROBES,
			    g_ofile, g_cmdc, objv) != 0)
				dfatal(NULL); /* dtrace_errmsg() only */
		}

		dtrace_close(g_dtp);
		return g_status;

	case DMODE_LIST:
		if (g_ofile != NULL && (g_ofp = fopen(g_ofile, "a")) == NULL)
			fatal("failed to open output file '%s'", g_ofile);

		oprintf("%5s %10s %17s %33s %s\n",
		    "ID", "PROVIDER", "MODULE", "FUNCTION", "NAME");

		for (i = 0; i < g_cmdc; i++)
			list_prog(&g_cmdv[i]);

		if (g_cmdc == 0) {
			if (dtrace_probe_iter(g_dtp, NULL, list_probe, NULL) < 0)
				dfatal(NULL); /* dtrace_errmsg() only */
		}

		dtrace_close(g_dtp);
		return g_status;

	case DMODE_HEADER:
		if (g_cmdc == 0) {
			fprintf(stderr, "%s: -h requires one or more "
			    "scripts or enabling options\n", g_pname);
			dtrace_close(g_dtp);
			return E_USAGE;
		}

		if (g_ofile == NULL) {
			char *p;

			if (g_cmdc > 1) {
				fprintf(stderr, "%s: -h requires an "
				    "output file if multiple scripts are "
				    "specified\n", g_pname);
				dtrace_close(g_dtp);
				return E_USAGE;
			}

			if ((p = strrchr(g_cmdv[0].dc_arg, '.')) == NULL ||
			    strcmp(p, ".d") != 0) {
				fprintf(stderr, "%s: -h requires an "
				    "output file if no scripts are "
				    "specified\n", g_pname);
				dtrace_close(g_dtp);
				return E_USAGE;
			}

			p[0] = '\0'; /* strip .d suffix */
			g_ofile = p = g_cmdv[0].dc_ofile;
			snprintf(p, sizeof(g_cmdv[0].dc_ofile),
			    "%s.h", basename(g_cmdv[0].dc_arg));
		}

		if ((g_ofp = fopen(g_ofile, "w")) == NULL)
			fatal("failed to open header file '%s'", g_ofile);

		oprintf("/*\n * Generated by dtrace(1M).\n */\n\n");

		if (dtrace_program_header(g_dtp, g_ofp, g_ofile) != 0 ||
		    fclose(g_ofp) == EOF)
			dfatal("failed to create header file %s", g_ofile);

		dtrace_close(g_dtp);
		return g_status;
	}

	/*
	 * If -Z was not specified and no probes have been matched, no probe
	 * criteria was specified on the command line and we abort.
	 */
	if (g_total == 0 && !(g_cflags & DTRACE_C_ZDEFS))
		dfatal("no probes %s\n", g_cmdc ? "matched" : "specified");

	/*
	 * Start tracing.
	 */
	if (g_intr)
		goto out;
	go();

	dtrace_getopt(g_dtp, "flowindent", &opt);
	g_flowindent = opt != DTRACEOPT_UNSET;

	dtrace_getopt(g_dtp, "quiet", &opt);
	g_quiet = opt != DTRACEOPT_UNSET;

	dtrace_getopt(g_dtp, "destructive", &opt);
	if (opt != DTRACEOPT_UNSET)
		notice("allowing destructive actions\n");

	/*
	 * Now that tracing is active and we are ready to consume trace data,
	 * continue any grabbed or created processes, setting them running
	 * using the /proc control mechanism inside of libdtrace.
	 */
	for (i = 0; i < g_psc; i++)
		dtrace_proc_continue(g_dtp, g_psv[i]);
	if (g_intr)
		goto release_procs;

	g_pslive = g_psc; /* count for prochandler() */

	do {
		if ((g_newline) && (!g_testing)) {
			/*
			 * Output a newline just to make the output look
			 * slightly cleaner.  Note that we do this even in
			 * "quiet" mode...  We don't do it when testing, as the
			 * newline appears at an essentially random position in
			 * the output.
			 */
			oprintf("\n");
			g_newline = 0;
		}

		if (done == DONE_SAW_EXIT || g_intr ||
		    (g_psc != 0 && g_pslive <= 0)) {
			done = DONE_SAW_END;
			if (dtrace_stop(g_dtp) == -1)
				dfatal("couldn't stop tracing");
		}

		switch (dtrace_work(g_dtp, g_ofp, chew, chewrec, NULL)) {
		case DTRACE_WORKSTATUS_DONE:
			if (done != DONE_SAW_END)
				done = DONE_SAW_EXIT;
			break;
		case DTRACE_WORKSTATUS_OKAY:
			break;
		default:
			if (!g_impatient && dtrace_errno(g_dtp) != EINTR)
				dfatal("processing aborted");
		}

		if (g_ofp != NULL && fflush(g_ofp) == EOF)
			clearerr(g_ofp);
	} while (done != DONE_SAW_END);

	oprintf("\n");

	if (!g_impatient) {
		if (dtrace_aggregate_print(g_dtp, g_ofp, NULL) == -1 &&
		    dtrace_errno(g_dtp) != EINTR)
			dfatal("failed to print aggregations");
	}

release_procs:
	for (i = 0; i < g_psc; i++)
		dtrace_proc_release(g_dtp, g_psv[i]);

out:
	dtrace_close(g_dtp);

	free(g_argv);
	free(g_cmdv);
	free(g_psv);

	return g_status;
}
