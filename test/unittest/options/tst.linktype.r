link pass:
objdump recognizes elf
link pass: -xlinktype=elf
objdump recognizes elf
link FAIL: -xlinktype=dof
link FAIL: -xlinktype=foo
-- @@stderr --
dtrace: failed to link script prov: link type 1 (DOF) no longer supported
dtrace: failed to set -x linktype: Invalid value for specified option
