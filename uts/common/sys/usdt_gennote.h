/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _SYS_USDT_GENNOTE_H_
#define	_SYS_USDT_GENNOTE_H_

#include "usdt_internal.h"
#include "usdt_note_defs.h"

#ifdef __aarch64__
# define _DT_FN_MOD
# define _DT_FN_CONSTRAINT	S
#else
# define _DT_FN_MOD		p
# define _DT_FN_CONSTRAINT	s
#endif

#ifndef _DT_ARG_CONSTRAINT
# define _DT_ARG_CONSTRAINT	nor
#endif

#define __dt_s(...)		# __VA_ARGS__
#define _dt_s(...)		__dt_s(__VA_ARGS__)

#define _DT_ASM(...)		__dt_s(__VA_ARGS__) "\n"

/* XXX */
#define _DT_INTTYPE(expr) \
  __typeof( \
    __builtin_choose_expr( \
      ((__builtin_classify_type(expr) + 3) & -4) == 4, (expr), 0UL \
    ) \
  )
#define _DT_SIGNED(expr) \
  ((_DT_INTTYPE(expr))-1 < (_DT_INTTYPE(expr))0)
#define _DT_ARRAY(expr) \
  (__builtin_classify_type(expr) == 5 || __builtin_classify_type(expr) == 14)
#define _DT_SIZEOF(expr) \
  (_DT_ARRAY(expr) ? sizeof(void *) : sizeof(expr))
#define _DT_TCLASS(expr) \
  (__builtin_classify_type(expr) & 0x7f)

#define _DT_ARGSST(arg) \
  ( \
    (_DT_SIGNED(arg) ? -1 : 1) * \
    (((int)(_DT_SIZEOF(arg) << 8)) + ((int)_DT_TCLASS(arg))) \
  )

/*
 * Assembler macros to construct the sign/size/type prefix for arguments:
 *   - '-' if the argument is signed (omitted for unsigned)
 *   - Then the size follows (in bytes)
 *   - Then 'f' if the argument is floating point (omitted if not)
 *   - Then '@'
 *
 * This prefix is derived from the value calculated using the _DT_ARGSST(arg)
 * macro defined above.
 */
#define _DT_ASM_DEFINE_MACROS \
  _DT_ASM(.altmacro) \
  _DT_ASM(.macro _SST x y z) \
  _DT_ASM(.iflt \\x) \
  _DT_ASM(.ascii "-") \
  _DT_ASM(.endif) \
  _DT_ASM(.ascii "\y") \
  _DT_ASM(.ifc 8,\\z) \
  _DT_ASM(.ascii "f") \
  _DT_ASM(.endif) \
  _DT_ASM(.ascii "@") \
  _DT_ASM(.endm) \
  _DT_ASM(.macro SST x) \
  _DT_ASM(_SST \\x, %%(-(\\x*((\\x>0)-(\\x<0)))>>8), %%((\\x))&(0xff)) \
  _DT_ASM(.endm)

#define _DT_ASM_UNDEF_MACROS \
  _DT_ASM(.purgem SST) \
  _DT_ASM(.purgem _SST)

#define _DT_SEP_NONE
#define _DT_SEP_COMMA	,

#define _DT_ARG_TYPE(n, arg)	unsigned long

#define _DT_ASM_ARGDATA(n, arg) \
  _DT_ASM(   SST %c[SST##n]) \
  _DT_ASM(   .ascii _dt_s(%[VAL##n]))

#define _DT_ASM_ARGDEF(n, arg) \
  [SST##n] "n" (_DT_ARGSST(arg)), \
  [VAL##n] _dt_s(_DT_ARG_CONSTRAINT) (arg),

#define _DT_ASM_OUT_1(n, arg)
#define _DT_ASM_OUT_2(n, arg)	"+m" (*arg)

#define _DT_CLOBBER_1	"memory"
#define _DT_CLOBBER_2

#define _USDT_PROBE__(prv, prb, typ, ...) \
  do { \
    extern void __usdt##typ##_##prv##___##prb(__DT_APPLY_DEFAULT(_DT_ARG_TYPE, void, ## __VA_ARGS__)); \
    __asm__ __volatile__ (\
      _DT_ASM_DEFINE_MACROS \
      _DT_ASM(0: nop) \
      _DT_ASM(	 .pushsection _USDT_SECT_NAME,"?",@note) \
      _DT_ASM(	 .balign 4) \
      _DT_ASM(	 .4byte 2f-1f) \
      _DT_ASM(	 .4byte 4f-3f) \
      _DT_ASM(	 .4byte typ) \
      _DT_ASM(1: .asciz _USDT_TP_NOTE_NAME) \
      _DT_ASM(2: .balign 4) \
      _DT_ASM(3: .8byte 0b) \
      _DT_ASM(	 .8byte %_DT_FN_MOD[fn]) \
      _DT_ASM(	 .asciz _dt_s(prv)) \
      _DT_ASM(	 .asciz _dt_s(prb)) \
      _DT_ASM(	 .byte __DT_NARGS(__VA_ARGS__)) \
      __DT_APPLY_SEP(_DT_ASM_ARGDATA, _DT_ASM(   .ascii " "), ## __VA_ARGS__) \
      _DT_ASM(	 .byte 0) \
      _DT_ASM(4: .balign 4) \
      _DT_ASM(	 .popsection) \
      _DT_ASM_UNDEF_MACROS \
      : __DT_APPLY(__DT_CAT(_DT_ASM_OUT_, typ), ## __VA_ARGS__) \
      : __DT_APPLY(_DT_ASM_ARGDEF, ## __VA_ARGS__) \
         [fn] _dt_s(_DT_FN_CONSTRAINT) (__func__) \
      : __DT_CAT(_DT_CLOBBER_, typ) \
    ); \
  } while (0)

#define _USDT_PROBE_(prv, prb, typ, ...) \
		_USDT_PROBE__(prv, prb, typ, ## __VA_ARGS__)
#define _USDT_PROBE(prv, prb, ...) \
		_USDT_PROBE_(prv, prb, _USDT_TP_NOTE_TYPE, ## __VA_ARGS__)
#define _USDT_PROBE_ENABLED(prv, prb, ...) \
		_USDT_PROBE_(prv, prb, _USDT_EN_NOTE_TYPE, ## __VA_ARGS__)

#endif /* _SYS_USDT_GENNOTE_H_ */
