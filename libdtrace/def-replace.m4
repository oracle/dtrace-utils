/*
 * Run before all other M4 scripts in the replacement path,
 * this script arranges to define macros corresponding to each
 * C preprocessor macro in the preprocessed input that is
 * defined on this system.  Macros that are not defined yield
 * an error.
 */
m4_define(`__def_replace',`m4_ifelse(m4_translit(`$1',`"',`'),`$2',`m4_errprint(`Macro $1 has no definition on this system.
') m4_m4exit(1)',`m4_define(m4_translit(`$1',`"',`'),$2)')')
