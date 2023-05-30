/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libproc.h>
#include <assert.h>
#include <tracefs.h>

/*
 * Return a uprobe spec for a given address in a given process handle.
 */
char *
uprobe_spec_by_addr(ps_prochandle *P, uint64_t addr, prmap_t *mapp_)
{
	char			*spec = NULL;
	const prmap_t		*mapp, *first_mapp;
	char			*mapfile_name = NULL;

	mapp = Paddr_to_map(P, addr);
	if (mapp == NULL)
		goto out;

	first_mapp = mapp->pr_file->first_segment;

	/*
	 * Use a name in /proc/$pid/map_files: this will work even if the
	 * destination is in a different filesystem namespace.  Never use the
	 * absolute path: not only might this not exist, but an *entirely
	 * different file* might be found there in the namespace in which we
	 * are running: prf_mapname is derived from /proc/$pid/maps, and the
	 * names in there are not relative to the namespace of the reader
	 * at all.
	 */
	mapfile_name = Pmap_mapfile_name(P, mapp);
	if (!mapfile_name)
		goto out;

	/*
	 * No need for error-checking here: we do the same on error
	 * and success.
	 */
	asprintf(&spec, "%s:0x%lx", mapfile_name, addr - first_mapp->pr_vaddr);

	if (mapp_)
		memcpy(mapp_, mapp, sizeof(prmap_t));

out:
	free(mapfile_name);
	return spec;
}

static const char hexdigits[] = "0123456789abcdef";

/*
 * Encode a NAME suitably for representation in a uprobe.  All non-alphanumeric,
 * non-_ characters are replaced with __XX where XX is the hex encoding of the
 * ASCII code of the byte. __ itself is replaced with ___.
 */
char *
uprobe_encode_name(const char *name)
{
	const char *p = name;
	char *out_p;
	char *encoded;
	size_t sz = strlen(name);

	/*
	 * Compute size changes needed.
	 */

	while ((p = strstr(p, "__")) != NULL) {
		sz++;
		p += 2;
	}

	for (p = name; *p != '\0'; p++) {
		if (!isalpha(*p) && !isdigit(*p) && *p != '_')
			sz += 3;
	}

	encoded = malloc(sz + 1);
	if (!encoded)
		return NULL;
	out_p = encoded;

	/* Apply translations.  */

	for (p = name; *p != '\0'; p++) {
		int hexencode = 0, underencode = 0;

		if (!isalpha(*p) && !isdigit(*p) && *p != '_')
			hexencode = 1;
		if (p[0] == '_' && p[1] == '_' && p[2] != '\0')
			underencode = 1;

		if (underencode) {
			*out_p++ = '_';
			*out_p++ = '_';
			*out_p++ = '_';
			p++;
			continue;
		}

		if (hexencode) {
			*out_p++ = '_';
			*out_p++ = '_';
			*out_p++ = hexdigits[*p >> 4];
			*out_p++ = hexdigits[*p & 0xf];
		}
		else
			*out_p++ = *p;
	}
	*out_p = '\0';

	return encoded;
}

/*
 * Decode a NAME: the converse of uprobe_encode_name.
 */
char *
uprobe_decode_name(const char *name)
{
	const char *p = name;
	char *new_p, *out_p;
	char *decoded;
	size_t sz = strlen(name);

	/*
	 * Compute size changes needed.
	 */

	while ((p = strstr(p, "__")) != NULL) {
		if (p[3] == '_') {
			sz--;
			p += 3;
		}
		else if (strspn(&p[2], hexdigits) >= 2) {
			sz -= 3;
			p += 4;
		}
	}

	decoded = malloc(sz + 1);
	if (!decoded)
		return NULL;
	out_p = decoded;

	/* Apply translations.  */

	p = name;
	while ((new_p = strstr(p, "__")) != NULL) {

		/*
		 * Copy unchanged bytes.
		 */
		memcpy(out_p, p, new_p - p);
		out_p += new_p - p;
		p = new_p;

		if (p[3] == '_') {
			*out_p++ = '_';
			*out_p++ = '_';
			p += 3;
		} else if (strspn(&p[2], hexdigits) >= 2) {
			if (isdigit(p[2]))
				*out_p = (p[2] - '0') << 4;
			else
				*out_p = (p[2] - 'a' + 10) << 4;
			if (isdigit(p[3]))
				*out_p += p[3] - '0';
			else
				*out_p += p[3] - 'a' + 10;
			p += 4;
			out_p++;
		}
		else {
			*out_p++ = '_';
			*out_p++ = '_';
			p += 2;
		}
	}
	/*
	 * Copy the remainder.
	 */
	strcpy(out_p, p);

	return decoded;
}

char *
uprobe_name(dev_t dev, ino_t ino, uint64_t addr, int isret, int is_enabled)
{
	char	*name;

	if (asprintf(&name, "dt_pid%s/%c_%llx_%llx_%lx", is_enabled?"_is_enabled":"",
		     isret ? 'r' : 'p', (unsigned long long)dev,
		     (unsigned long long)ino, (unsigned long)addr) < 0)
		return NULL;

	return name;
}

/*
 * Create a uprobe for a given device, address, and spec: the uprobe may be a
 * uretprobe.  Return the probe's name as a new dynamically-allocated string, or
 * NULL on error.  If prv/mod/fun/prb are all set, they are passed down as the
 * name of the corresponding DTrace probe.
 */
char *
uprobe_create_named(dev_t dev, ino_t ino, uint64_t addr, const char *spec, int isret,
		    int is_enabled, const char *prv, const char *mod, const char *fun,
		    const char *prb)
{
	int	fd = -1;
	int	rc = -1;
	char	*name, *args = NULL;

	if (prv && mod && fun && prb) {
		char *eprv, *emod, *efun, *eprb;
		int failed = 0;

		eprv = uprobe_encode_name(prv);
		emod = uprobe_encode_name(mod);
		efun = uprobe_encode_name(fun);
		eprb = uprobe_encode_name(prb);

		if (eprv && emod && efun && eprb) {
			if (asprintf(&args, "P%s=\\1 M%s=\\2 F%s=\\3 N%s=\\4",
				     eprv, emod, efun, eprb) < 0)
				failed = 1;
		} else
			failed = 1;

		free(eprv);
		free(emod);
		free(efun);
		free(eprb);

		if (failed)
			return NULL;
	}

	name = uprobe_name(dev, ino, addr, isret, is_enabled);
	if (!name)
		goto out;

	/* Add the uprobe. */
	fd = open(TRACEFS "uprobe_events", O_WRONLY | O_APPEND);
	if (fd == -1)
		goto out;

	rc = dprintf(fd, "%c:%s %s %s\n", isret ? 'r' : 'p', name, spec,
		     args ? args : "");

out:
	if (fd != -1)
		close(fd);
	free(args);
	if (rc < 0) {
		free(name);
		return NULL;
	}

	return name;
}

/*
 * Like uprobe_create, but do not specify the name of a corresponding DTrace
 * probe.  (Used when the caller already knows what probe will be needed, and
 * there is no possibility of another DTrace having to pick it up from the
 * systemwide uprobe list.)
 */
char *
uprobe_create(dev_t dev, ino_t ino, uint64_t addr, const char *spec, int isret,
	int is_enabled)
{
	return uprobe_create_named(dev, ino, addr, spec, isret, is_enabled,
				   NULL, NULL, NULL, NULL);
}

/*
 * Create a uprobe given a particular process and address.  Return the probe's
 * name as a new dynamically-allocated string, or NULL on error.  If
 * prv/mod/fun/prb are set, they are passed down as the name of the
 * corresponding DTrace probe.
 */
char *
uprobe_create_from_addr(ps_prochandle *P, uint64_t addr, int is_enabled,
			const char *prv, const char *mod, const char *fun,
			const char *prb)
{
	char *spec;
	char *name;
	prmap_t mapp;

	spec = uprobe_spec_by_addr(P, addr, &mapp);
	if (!spec)
		return NULL;

	name = uprobe_create_named(mapp.pr_dev, mapp.pr_inum, addr, spec, 0,
				   is_enabled, prv, mod, fun, prb);
	free(spec);
	return name;
}

/*
 * Destroy a uprobe for a given device and address.
 */
int
uprobe_delete(dev_t dev, ino_t ino, uint64_t addr, int isret, int is_enabled)
{
	int	fd = -1;
	int	rc = -1;
	char	*name;

	name = uprobe_name(dev, ino, addr, isret, is_enabled);
	if (!name)
		goto out;

	fd = open(TRACEFS "uprobe_events", O_WRONLY | O_APPEND);
	if (fd == -1)
		goto out;

	rc = dprintf(fd, "-:%s\n", name);

out:
	if (fd != -1)
		close(fd);
	free(name);

	return rc < 0 ? -1 : 0;
}
