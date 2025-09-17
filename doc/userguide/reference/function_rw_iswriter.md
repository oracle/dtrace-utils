
# rw\_iswriter

Checks whether a writer is holding or waiting for the specified reader-writer lock.

```
int rw_iswriter(vmlinux`rwlock_t **rwlock*)
```

The `rw_iswriter` function returns non zero if a writer is holding or waiting for the specified reader-writer lock \(*rwlock*\). If the lock is held only by readers and no writer is blocked, or if the lock isn't held at all, `rw_iswriter` returns zero.

## How to use rw\_iswriter to check whether a writer is holding or waiting for a specified reader-writer lock

The example contains two clauses. The first clause triggers for when the `_raw_write_lock` is entered, and uses `rw_iswriter` function to print whether a lock is held. At this stage, no lock is held, so the output returns `0`. When the `_raw_write_lock` returns, a lock is held and the `rw_iswriter` function returns `1` and exits.

```
fbt:vmlinux:_raw_write_lock:entry
{
         self->wlock = (rwlock_t *)arg0;
         printf("write entry  %x\n", 0 != rw_iswriter(self->wlock));
}
 
fbt:vmlinux:_raw_write_lock:return
/self->wlock/
{
         printf("write return %x\n", 0 != rw_iswriter(self->wlock));
         exit(0)
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

