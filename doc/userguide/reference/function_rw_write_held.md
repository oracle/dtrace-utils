
# rw\_write\_held

Checks whether the specified reader-writer lock is held by a writer.

```
int rw_write_held(vmlinux`rwlock_t **rwlock*)
```

The `rw_write_held` function returns non zero if the specified reader-writer lock \(*rwlock*\) is held by a writer. If the lock is held only by readers or isn't held at all, `rw_write_held` returns zero.

## How to use rw\_write\_held to check whether a writer is holding a specified reader-writer lock

The example uses two clauses. The first clause triggers for when the `_raw_write_lock` is entered, and uses `rw_write_held` function to print whether a write lock is held. At this stage, no lock is held, so the output returns `0`. When the `_raw_write_lock` returns, a lock is held and the `rw_write_held` function returns `1` and the script exits.

```
 fbt:vmlinux:_raw_write_lock:entry
 {
         self->wlock = (rwlock_t *)arg0;
         printf("write entry  %x\n", 0 != rw_write_held(self->wlock));
 }
 
 fbt:vmlinux:_raw_write_lock:return
 /self->wlock/
 {
         printf("write return %x\n", 0 != rw_write_held(self->wlock));
         exit(0)
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

