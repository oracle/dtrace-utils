                   FUNCTION:NAME
                          :BEGIN 
               __vfs_write:entry 
              vmlinux`__vfs_write+{ptr}
              vmlinux`ksys_write+{ptr}
              vmlinux`__x64_sys_write+{ptr}
              vmlinux`do_syscall_64+{ptr}
              vmlinux`entry_SYSCALL_64+{ptr}


-- @@stderr --
dtrace: script 'test/unittest/stack/tst.stack_fbt.d' matched 2 probes
dtrace: allowing destructive actions
