/* Copyright © 2006, 2012, Oracle and/or its affiliates. All rights reserved.
   This file is free software; the Free Software Foundation
   gives unlimited permission to copy and/or distribute it,
   with or without modifications, as long as this notice is preserved. */

/* quote(args) - convert args to single-quoted string */
m4_define([[m4_quote]], [[m4_ifelse([[$#]], [[0]], [[]], [[[[$*]]]])]])

/* m4_dquote(args) - convert args to quoted list of quoted strings */
m4_define([[m4_dquote]], [[[[$@]]]])

# m4_foreachq(x, `item_1, item_2, ..., item_n', stmt)
#   quoted list, improved version
m4_define([[m4_foreachq]], [[m4_pushdef([[$1]])_$0($@)m4_popdef([[$1]])]])
m4_define([[_m4_arg1q]], [[[[$1]]]])
m4_define([[_m4_rest]], [[m4_ifelse([[$#]], [[1]], [[]], [[m4_dquote(m4_shift($@))]])]])
m4_define([[_m4_foreachq]], [[m4_ifelse([[$2]], [[]], [[]],
  [[m4_define([[$1]], _m4_arg1q($2))$3[[]]$0([[$1]], _m4_rest($2), [[$3]])]])]])
