/*
 * Oracle Linux DTrace; DOF storage for later probe removal.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DOF_STASH_H
#define	_DOF_STASH_H

#include <sys/types.h>
#include <stdint.h>

#include <dt_list.h>
#include "dof_parser.h"

typedef struct dof_parsed_list {
	dt_list_t list;
	dof_parsed_t *parsed;
} dof_parsed_list_t;

int dof_stash_init(const char *statedir);

int dof_stash_push_parsed(dt_list_t *accum, dof_parsed_t *parsed);
int dof_stash_write_parsed(pid_t pid, dev_t dev, ino_t ino, dt_list_t *accum);
void dof_stash_free(dt_list_t *accum);

int dof_stash_add(pid_t pid, dev_t dev, ino_t ino, const dof_helper_t *dh,
		  const void *dof, size_t size);
int dof_stash_remove(pid_t pid, int gen);

void dof_stash_prune_dead(void);

int
reparse_dof(int out, int in,
	    int (*reparse)(int pid, int out, int in, dev_t dev, ino_t inum,
			   dof_helper_t *dh, const void *in_buf,
			   size_t in_bufsz, int reparsing),
	    int force);

#endif
