/*
 * Run after the C preprocessor output has been sucked in and transformed
 * into M4 macro definitions, this script provides a macro which permits
 * text to be conditionally replaced depending on which kernel definitions
 * are being generated for.
 */

/*
 * define_for_kernel([[version]]..., [[string]], [[result]], ([[otherwise]]))
 * expands to a macro that replaces [[string]] with [[result]], but only on
 * kernels in the quoted list [[version]]; if [[otherwise]] is provided,
 * other kernels get this expansion instead.
 */

m4_define([[define_for_kernel]],[[m4_pushdef([[__found]],nil)m4_dnl
m4_foreachq([[kernel]],m4_quote($1),[[m4_ifelse(SUBST_KERNEL,kernel,[[m4_define([[__found]], t)]])]])m4_dnl
m4_ifelse($#,3,[[m4_ifelse(__found,t,[[m4_define(m4_quote($2),m4_quote($3))]])]],[[m4_ifelse(__found,t,[[m4_define(m4_quote($2),m4_quote($3))]],[[m4_define(m4_quote($2),m4_quote($4))]])]])m4_dnl
m4_popdef([[__found]])]])m4_dnl
