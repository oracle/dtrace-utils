FUNCTION                                 
 => read                                  syscall: 0

   -> __x64_sys_read                      fbt: 0

   <- __x64_sys_read                      fbt: 1

 <= read                                  syscall: 1

 => read                                  syscall: 2

   -> __x64_sys_read                      fbt: 2

   <- __x64_sys_read                      fbt: 3

 <= read                                  syscall: 3

 => read                                  syscall: 4

   -> __x64_sys_read                      fbt: 4

   <- __x64_sys_read                      fbt: 5

 <= read                                  syscall: 5

 => read                                  syscall: 6

   -> __x64_sys_read                      fbt: 6

   <- __x64_sys_read                      fbt: 7

 <= read                                  syscall: 7

 => read                                  syscall: 8

   -> __x64_sys_read                      fbt: 8

   <- __x64_sys_read                      fbt: 9

 <= read                                  syscall: 9


-- @@stderr --
dtrace: script 'test/unittest/options/tst.flowindent.d' matched 7 probes
