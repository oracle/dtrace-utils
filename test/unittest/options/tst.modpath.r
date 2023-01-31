                   FUNCTION:NAME
                          :BEGIN 

sanity check on DTrace passed
DTrace correctly fails when modpath is an empty directory
                   FUNCTION:NAME
                          :BEGIN 

DTrace works with modpath and a linked CTF archive
-- @@stderr --
dtrace: description 'BEGIN ' matched 1 probe
dtrace: description 'BEGIN ' matched 1 probe
dtrace: could not enable tracing: Module does not contain any CTF data
dtrace: description 'BEGIN ' matched 1 probe
