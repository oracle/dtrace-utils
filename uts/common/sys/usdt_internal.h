/*
 * Hide away all the terrible macro magic.
 *
 * Oracle Linux DTrace.
 * Copyright (c) 2016, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _LINUX_SDT_INTERNAL_H_
#define _LINUX_SDT_INTERNAL_H_

/*
 * This counts the number of args.
 */
#define __DT_NARGS_SEQ(dummy,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,N,...) N
#define __DT_NARGS(...)						\
	__DT_NARGS_SEQ(dummy, ##__VA_ARGS__, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

/*
 * This will let macros expand before concatting them.
 */
#define __DT_PRIMITIVE_CAT(x, y) x ## y
#define __DT_CAT(x, y) __DT_PRIMITIVE_CAT(x, y)

#define __DT_COMMA ,
#define __DT_NO_COMMA
#define __DT_NONE(x)

/*
 * This will call two macros on each argument-pair passed in (the first two args
 * are the names of the macros to call).  Its TYPE and NAME variants will throw
 * away the name and type arguments, respectively. __DT_*_APPLY_NOCOMMA
 * are like DTRACE_*_APPLY, but also omit the comma between arguments in the
 * expansion of the macro.  DTRACE_TYPE_APPLY_DEFAULT lets you specify a default
 * if no variadic args are provided.
 */
#define __DT_DOUBLE_APPLY(type_macro, arg_macro, ...)		\
	__DT_CAT(__DT_DOUBLE_APPLY_,				\
		     __DT_NARGS(__VA_ARGS__))(type_macro,		\
						  arg_macro, __DT_COMMA, \
						  __DT_COMMA, , ## __VA_ARGS__)
#define __DT_DOUBLE_APPLY_NOCOMMA(type_macro, arg_macro, ...)		\
	__DT_CAT(__DT_DOUBLE_APPLY_,				\
		     __DT_NARGS(__VA_ARGS__))(type_macro,		\
						  arg_macro, __DT_NO_COMMA, \
						  __DT_NO_COMMA, , ## __VA_ARGS__)
#define __DT_TYPE_APPLY(type_macro, ...)				\
	__DT_CAT(__DT_DOUBLE_APPLY_,				\
		     __DT_NARGS(__VA_ARGS__))(type_macro,		\
						  __DT_NONE, __DT_NO_COMMA, \
						  __DT_COMMA, , ## __VA_ARGS__)
#define __DT_TYPE_APPLY_NOCOMMA(type_macro, ...)			\
	__DT_CAT(__DT_DOUBLE_APPLY_,				\
		     __DT_NARGS(__VA_ARGS__))(type_macro,		\
						  __DT_NONE, __DT_NO_COMMA, \
						  __DT_NO_COMMA, , ## __VA_ARGS__)
#define __DT_TYPE_APPLY_DEFAULT(type_macro, def, ...)		\
	__DT_CAT(__DT_DOUBLE_APPLY_,				\
		     __DT_NARGS(__VA_ARGS__))(type_macro,		\
						  __DT_NONE, __DT_NO_COMMA, \
						  __DT_COMMA, def, ## __VA_ARGS__)
#define __DT_ARG_APPLY(arg_macro, ...)				\
	__DT_CAT(__DT_DOUBLE_APPLY_,				\
		     __DT_NARGS(__VA_ARGS__))(__DT_NONE,	\
						  arg_macro, __DT_NO_COMMA,	\
						  __DT_COMMA, , ## __VA_ARGS__)
#define __DT_DOUBLE_APPLY_0(t, a, comma_t, comma_a, def) def
#define __DT_DOUBLE_APPLY_2(t, a, comma_t, comma_a, def, type1, arg1) \
	t(type1) comma_t a(arg1)
#define __DT_DOUBLE_APPLY_4(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2)
#define __DT_DOUBLE_APPLY_6(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3)
#define __DT_DOUBLE_APPLY_8(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4)
#define __DT_DOUBLE_APPLY_10(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5)
#define __DT_DOUBLE_APPLY_12(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6)
#define __DT_DOUBLE_APPLY_14(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7)
#define __DT_DOUBLE_APPLY_16(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8)
#define __DT_DOUBLE_APPLY_18(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9)
#define __DT_DOUBLE_APPLY_20(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9, typea, arga) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9) comma_a t(typea) comma_t a(arga)
#define __DT_DOUBLE_APPLY_22(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9, typea, arga, typeb, argb) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9) comma_a t(typea) comma_t a(arga) comma_a \
	t(typeb) comma_t a(argb)
#define __DT_DOUBLE_APPLY_24(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9, typea, arga, typeb, argb, typec, argc) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9) comma_a t(typea) comma_t a(arga) comma_a \
	t(typeb) comma_t a(argb) comma_a t(typec) comma_t a(argc)
#define __DT_DOUBLE_APPLY_26(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9, typea, arga, typeb, argb, typec, argc, typed, argd) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9) comma_a t(typea) comma_t a(arga) comma_a \
	t(typeb) comma_t a(argb) comma_a t(typec) comma_t a(argc) comma_a \
	t(typed) comma_t a(argd)
#define __DT_DOUBLE_APPLY_28(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9, typea, arga, typeb, argb, typec, argc, typed, argd, typee, arge) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9) comma_a t(typea) comma_t a(arga) comma_a \
	t(typeb) comma_t a(argb) comma_a t(typec) comma_t a(argc) comma_a \
	t(typed) comma_t a(argd) comma_a t(typee) comma_t a(arge)
#define __DT_DOUBLE_APPLY_30(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9, typea, arga, typeb, argb, typec, argc, typed, argd, typee, arge, typef, argf) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9) comma_a t(typea) comma_t a(arga) comma_a \
	t(typeb) comma_t a(argb) comma_a t(typec) comma_t a(argc) comma_a \
	t(typed) comma_t a(argd) comma_a t(typee) comma_t a(arge) comma_a \
	t(typef) comma_t a(argf)
#define __DT_DOUBLE_APPLY_32(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9, typea, arga, typeb, argb, typec, argc, typed, argd, typee, arge, typef, argf, typeg, argg) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9) comma_a t(typea) comma_t a(arga) comma_a \
	t(typeb) comma_t a(argb) comma_a t(typec) comma_t a(argc) comma_a \
	t(typed) comma_t a(argd) comma_a t(typee) comma_t a(arge) comma_a \
	t(typef) comma_t a(argf) comma_a t(typeg) comma_t a(argg)
#define __DT_DOUBLE_APPLY_34(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9, typea, arga, typeb, argb, typec, argc, typed, argd, typee, arge, typef, argf, typeg, argg, typeh, argh) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9) comma_a t(typea) comma_t a(arga) comma_a \
	t(typeb) comma_t a(argb) comma_a t(typec) comma_t a(argc) comma_a \
	t(typed) comma_t a(argd) comma_a t(typee) comma_t a(arge) comma_a \
	t(typef) comma_t a(argf) comma_a t(typeg) comma_t a(argg) comma_a \
	t(typeh) comma_t a(argh)
#define __DT_DOUBLE_APPLY_36(t, a, comma_t, comma_a, def, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6, type7, arg7, type8, arg8, type9, arg9, typea, arga, typeb, argb, typec, argc, typed, argd, typee, arge, typef, argf, typeg, argg, typeh, argh, typei, argi) \
	t(type1) comma_t a(arg1) comma_a t(type2) comma_t a(arg2) comma_a \
	t(type3) comma_t a(arg3) comma_a t(type4) comma_t a(arg4) comma_a \
	t(type5) comma_t a(arg5) comma_a t(type6) comma_t a(arg6) comma_a \
	t(type7) comma_t a(arg7) comma_a t(type8) comma_t a(arg8) comma_a \
	t(type9) comma_t a(arg9) comma_a t(typea) comma_t a(arga) comma_a \
	t(typeb) comma_t a(argb) comma_a t(typec) comma_t a(argc) comma_a \
	t(typed) comma_t a(argd) comma_a t(typee) comma_t a(arge) comma_a \
	t(typef) comma_t a(argf) comma_a t(typeg) comma_t a(argg) comma_a \
	t(typeh) comma_t a(argh) comma_a t(typei) comma_t a(argi)

#define __DT_DOUBLE_APPLY_ERROR Error: type specified without arg.
#define __DT_DOUBLE_APPLY_1 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_3 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_5 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_7 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_9 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_11 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_13 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_15 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_17 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_19 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_21 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_23 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_25 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_27 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_29 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_31 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_33 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_35 __DT_DOUBLE_APPLY_ERROR
#define __DT_DOUBLE_APPLY_37 __DT_DOUBLE_APPLY_ERROR

#define __DT_UINTPTR_EACH(x) uintptr_t

#define __DT_UINTCAST_EACH(x) (uintptr_t)(x)
#define __DT_TYPE_EACH(x) ".ascii \"" __stringify(x) ",\"\n"

/*
 * Convert everything to the appropriate integral type, unless too large to fit
 * into any of them, in which case its address is taken instead.
 */

/*
 * This will call macro(idx, arg, sep) for each argument passed in (idx is the
 * index of the argument).
 * Use __DT_APPLY(macro, ...) to generate a simple concatenation of the result
 * of applying macro to each argument.
 * Use __DT_APPLY_SEP(macro, sep, ...) to generate output with the result of
 * applying macro to each argument separated by sep.
 * Use __DT_PPLY_COMMA(macro, ...) to generate output with the result of
 * applying maro to each argument separated by a comma.
 */
#define __DT_APPLY(macro, ...) __DT_CAT(__DT_APPLY_, __DT_NARGS(__VA_ARGS__))(macro, , , ## __VA_ARGS__)
#define __DT_APPLY_SEP(macro, sep, ...) __DT_CAT(__DT_APPLY_, __DT_NARGS(__VA_ARGS__))(macro, sep, , ## __VA_ARGS__)
#define __DT_APPLY_COMMA(macro, ...) __DT_CAT(__DT_APPLY_, __DT_NARGS(__VA_ARGS__))(macro, __DT_COMMA, , ## __VA_ARGS__)
#define __DT_APPLY_SEP_DEFAULT(macro, sep, def, ...) __DT_CAT(__DT_APPLY_, __DT_NARGS(__VA_ARGS__))(macro, sep, def, ## __VA_ARGS__)
#define __DT_APPLY_DEFAULT(macro, def, ...) __DT_CAT(__DT_APPLY_, __DT_NARGS(__VA_ARGS__))(macro, __DT_COMMA, def, ## __VA_ARGS__)

#define __DT_APPLY_0(m, sep, def) def
#define __DT_APPLY_1(m, sep, def, x1) m(1, x1)
#define __DT_APPLY_2(m, sep, def, x1, x2) m(1, x1) sep m(2, x2)
#define __DT_APPLY_3(m, sep, def, x1, x2, x3) m(1, x1) sep m(2, x2) sep m(3, x3)
#define __DT_APPLY_4(m, sep, def, x1, x2, x3, x4) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4)
#define __DT_APPLY_5(m, sep, def, x1, x2, x3, x4, x5) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5)
#define __DT_APPLY_6(m, sep, def, x1, x2, x3, x4, x5, x6) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6)
#define __DT_APPLY_7(m, sep, def, x1, x2, x3, x4, x5, x6, x7) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7)
#define __DT_APPLY_8(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8)
#define __DT_APPLY_9(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9)
#define __DT_APPLY_10(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9) sep m(10, x10)
#define __DT_APPLY_11(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9) sep m(10, x10) sep m(11, x11)
#define __DT_APPLY_12(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9) sep m(10, x10) sep m(11, x11) sep m(12, x12)
#define __DT_APPLY_13(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9) sep m(10, x10) sep m(11, x11) sep m(12, x12) sep m(13, x13)
#define __DT_APPLY_14(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9) sep m(10, x10) sep m(11, x11) sep m(12, x12) sep m(13, x13) sep m(14, x14)
#define __DT_APPLY_15(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9) sep m(10, x10) sep m(11, x11) sep m(12, x12) sep m(13, x13) sep m(14, x14) sep m(15, x15)
#define __DT_APPLY_16(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9) sep m(10, x10) sep m(11, x11) sep m(12, x12) sep m(13, x13) sep m(14, x14) sep m(15, x15) sep m(16, x16)
#define __DT_APPLY_17(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9) sep m(10, x10) sep m(11, x11) sep m(12, x12) sep m(13, x13) sep m(14, x14) sep m(15, x15) sep m(16, x16) sep m(17, x17)
#define __DT_APPLY_18(m, sep, def, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18) m(1, x1) sep m(2, x2) sep m(3, x3) sep m(4, x4) sep m(5, x5) sep m(6, x6) sep m(7, x7) sep m(8, x8) sep m(9, x9) sep m(10, x10) sep m(11, x11) sep m(12, x12) sep m(13, x13) sep m(14, x14) sep m(15, x15) sep m(16, x16) sep m(17, x17) sep m(18, x18)

#endif	/* _LINUX_SDT_INTERNAL_H */
