
# rw\_read\_held

Checks whether the specified reader-writer lock is held by a reader.

```
int rw_read_held(vmlinux`rwlock_t **rwlock*)
```

The `rw_read_held` function returns non zero if the specified reader-writer lock \(*rwlock*\) is held by a reader. If the lock is held only by writers or isn't held at all, `rw_read_held` returns zero.

## How to use rw\_iswriter to check whether a writer is holding or waiting for a specified reader-writer lock

The example includes two clauses. The first clause triggers for when the `_raw_read_lock` is entered, and uses `rw_read_held` function to print whether a lock is held. At this stage, no lock is held, so the output returns 0. When the `_raw_read_lock` returns, a lock is held and the `rw_read_held` function returns 1.

```
 fbt:vmlinux:_raw_read_lock:entry
 {
         self->rlock = (rwlock_t *)arg0;
         printf("read  entry  %x\n", 0 != rw_read_held(self->rlock));
 }
 
 fbt:vmlinux:_raw_read_lock:return
 /self->rlock/
 {
         printf("read  return %x\n", 0 != rw_read_held(self->rlock));
         exit(0);
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

