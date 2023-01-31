DTrace found the faulty D program in the specified subdirectory
-- @@stderr --
dtrace: invalid probe specifier BEGIN {exit(0)}: "subdirectory/faulty_D_program.d", line 2: undefined function name: nonexistent_action
