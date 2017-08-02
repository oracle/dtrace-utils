/*
 * Symbol table support for DTrace.
 */

/*
 * Oracle Linux DTrace.
 * Copyright © 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_SYMTAB_H
#define	_DT_SYMTAB_H

#include <gelf.h>
#include <dtrace.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * We cannot rely on ELF symbol table management at all times: in particular,
 * kernel symbols have no ELF symbol table.  Thus, this module implements a
 * simple, reasonably memory-efficient symbol table manager.  Once built up, the
 * symbol table needs sorting before it can be used for address->name lookup. It
 * can also be packed, which increases efficiency further but forbids further
 * modification.  (We do not define whether the 'more efficient' form increases
 * space- or time-efficiency.)
 */

typedef struct dt_symbol dt_symbol_t;
typedef struct dt_symtab dt_symtab_t;

extern dt_symtab_t *dt_symtab_create(void);
extern void dt_symtab_destroy(dt_symtab_t *symtab);
extern dt_symbol_t *dt_symbol_insert(dt_symtab_t *symtab, const char *name,
    GElf_Addr addr, GElf_Xword size, unsigned char info);
extern dt_symbol_t *dt_symbol_by_name(dt_symtab_t *symtab, const char *name);
extern dt_symbol_t *dt_symbol_by_addr(dt_symtab_t *symtab, GElf_Addr dts_addr);

extern void dt_symtab_sort(dt_symtab_t *symtab);
extern void dt_symtab_pack(dt_symtab_t *symtab);

extern const char *dt_symbol_name(dt_symtab_t *symtab, dt_symbol_t *symbol);
extern void dt_symbol_to_elfsym(dtrace_hdl_t *dtp, dt_symbol_t *symbol,
    GElf_Sym *elf_symp);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_SYMTAB_H */
