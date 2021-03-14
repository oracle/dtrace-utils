/*
 * Oracle Linux DTrace.
 * Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Run after the C preprocessor output has been sucked in and transformed
 * into M4 macro definitions, this script provides a macro which permits
 * text to be conditionally replaced depending on which kernel definitions
 * are being generated for.
 */

/*
 * if_arch([[arch]], [[string]]) expands to a macro that substitutes in
 * the [[string]] if the arch is that specified, otherwise nothing.
 */

m4_define([[if_arch]],[[m4_ifelse(SUBST_ARCH,m4_quote($1),m4_quote($2))]])

/*
 * define_for_kernel([[macro name]], [[(kver, value), (kver, value), ...]], [[default]])
 *
 * The macro takes a list of values for each kernel sorted from lowest to the
 * highest kernel version.  Given the target kernel version it tries to find
 * value for most recent kernel with lower version.  If no such kernel exist
 * the default value is used instead.
 *
 * The kver is produced by the m4_kver macro.
 *
 * If the value contains a macro argument variable like $1, then extra quoting
 * that prevents premature expansion is required.  See the m4_argq for more
 * details.
 */
m4_define([[__process_element]], m4_dnl
	[[m4_ifelse(m4_eval((SUBST_KERNEL[[ >= $1]]) && ($1 >= __found_version)), 1, m4_dnl
		[[m4_define([[__found]], [[$2]]) m4_dnl
		  m4_define([[__found_version]], [[$1]])]])]]) m4_dnl

m4_define([[__cat]], [[$1$2]]) m4_dnl

m4_define([[__define_for_kernel]], [[ m4_dnl
	m4_pushdef([[__found]], nil) m4_dnl
	m4_pushdef([[__found_version]], 0) m4_dnl
	m4_foreachq(kernel, m4_quote($2), [[ m4_dnl
		__cat([[__process_element]], kernel) m4_dnl
	]]) m4_dnl
	m4_ifelse(__found, nil, m4_dnl
		[[m4_define(m4_quote($1), [[$3]])]], m4_dnl
		[[m4_define(m4_quote($1), __found)]]) m4_dnl
	m4_popdef([[__found_version]]) m4_dnl
	m4_popdef([[__found]]) m4_dnl
]]) m4_dnl

m4_define([[define_for_kernel]], [[m4_divert(-1) __define_for_kernel($@) m4_divert(0)]])m4_dnl

/*
 * expand_for_kernel(name, [[(kver, value), (kver, value), ...]], [[default]])
 *
 * As define_for_kernel, but simply substitutes the result into the output
 * rather than defining a macro.  Every NAME should be unique.
 */
m4_define([[expand_for_kernel]], [[define_for_kernel(__expand_for_kernel_$1, m4_shift($@))__expand_for_kernel_$1]])m4_dnl
