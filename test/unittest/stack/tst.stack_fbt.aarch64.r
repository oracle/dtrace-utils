                   FUNCTION:NAME
                          :BEGIN 
               __vfs_write:entry 
              vmlinux`__vfs_write
              vmlinux`ksys_write+{ptr}
              vmlinux`__arm64_sys_write+{ptr}
              vmlinux`el0_svc_common+{ptr}
              vmlinux`el0_svc_handler+{ptr}
              vmlinux`el0_svc+{ptr}


-- @@stderr --
dtrace: script 'test/unittest/stack/tst.stack_fbt.d' matched 2 probes
dtrace: allowing destructive actions
