link pass:
objdump recognizes elf
link pass: -xlinktype=elf
objdump recognizes elf
link pass: -xlinktype=dof
objdump does NOT recognize file format
link FAIL: -xlinktype=foo
objdump does NOT recognize file format
-- @@stderr --
dtrace: failed to set -x linktype: Invalid value for specified option
