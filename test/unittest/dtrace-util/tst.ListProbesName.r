   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX     dtrace                                                     BEGIN
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                                read entry
 XX    syscall                                                read return
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                                read entry
 XX    syscall                                                read return
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                                read entry
 XX    syscall                                                read return
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                                read entry
 XX    syscall                                                read return
 XX    syscall                                               readv entry
 XX    syscall                                               readv return
 XX    syscall                                            readlink entry
 XX    syscall                                            readlink return
 XX    syscall                                           readahead entry
 XX    syscall                                           readahead return
 XX    syscall                                          readlinkat entry
 XX    syscall                                          readlinkat return
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    profile                                                     tick-1000
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                                read entry
 XX    syscall                                                read return
 XX    syscall                                               write entry
 XX    syscall                                               write return
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                                read entry
 XX    syscall                                                read return
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                                read entry
 XX    syscall                                                read return
 XX    syscall                                               readv entry
 XX    syscall                                               readv return
 XX    syscall                                            readlink entry
 XX    syscall                                            readlink return
 XX    syscall                                           readahead entry
 XX    syscall                                           readahead return
 XX    syscall                                          readlinkat entry
 XX    syscall                                          readlinkat return
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                               write entry
 XX    syscall                                               write return
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                                read entry
 XX    syscall                                                read return
   ID   PROVIDER            MODULE                          FUNCTION NAME
 XX    syscall                                                read entry
-- @@stderr --
dtrace: failed to match ::fight:: No probe matches description
dtrace: failed to match ::fight:: No probe matches description
dtrace: failed to match ::fight:: No probe matches description
dtrace: invalid probe specifier BEGIN{Printf("FOUND");}: undefined function name: Printf
dtrace: invalid probe specifier BEGIN/probename=="entry"/{Printf("FOUND");}: undefined function name: Printf
