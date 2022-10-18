/*
 * Oracle Linux DTrace; simple uprobe helper functions
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_UPROBES_H
#define	_UPROBES_H

#include <sys/types.h>
#include <inttypes.h>
#include <libproc.h>
#include <unistd.h>

extern char *uprobe_spec_by_addr(pid_t pid, ps_prochandle *P, uint64_t addr,
				 prmap_t *mapp);
extern char *uprobe_name(dev_t dev, ino_t ino, uint64_t addr, int isret);
extern char *uprobe_create_named(dev_t dev, ino_t ino, uint64_t addr,
				 const char *spec, int isret, const char *prv,
				 const char *mod, const char *fun,
				 const char *prb);
extern char *uprobe_create(dev_t dev, ino_t ino, uint64_t addr, const char *spec,
			   int isret);
extern char *uprobe_create_from_addr(pid_t pid, uint64_t addr, const char *prv,
				     const char *mod, const char *fun,
				     const char *prb);
extern int uprobe_delete(dev_t dev, ino_t ino, uint64_t addr, int isret);
extern char *uprobe_encode_name(const char *);
extern char *uprobe_decode_name(const char *);

#endif /* _UPROBES_H */
