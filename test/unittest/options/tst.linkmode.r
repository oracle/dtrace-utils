try
Linux version 

try -xlinkmode=static
Linux version 

try -xlinkmode=kernel
Linux version 

try -xlinkmode=foo
try -xlinkmode=dynamic
try -xlinkmode=dynamic -xknodefs
Linux version 

-- @@stderr --
dtrace: failed to set -x linkmode: Invalid value for specified option
dtrace: invalid probe specifier BEGIN { trace((string)&`linux_banner); exit(0); }: relocation remains against kernel symbol vmlinux`linux_banner (offset {ptr})
