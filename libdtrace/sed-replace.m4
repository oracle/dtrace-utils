divert(-1)
changecom(`/*',`*/')
define(`__sed_replace',`ifelse(translit(`$1',`"',`'),`$2',`@@@@SUBST-FAIL@@@@',`s/translit(`$1',`"',`@')/$2/')')
divert(0)
