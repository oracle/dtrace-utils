
# mutex\_owner

Returns the thread pointer to the current owner of the specified kernel mutex.

```
vmlinux`struct task_struct mutex_owner(vmlinux`struct mutex *)
```

The `mutex_owner` function returns the thread pointer of the current owner of the specified adaptive kernel mutex. `mutex_owner` returns `NULL` if the specified adaptive mutex is unowned or if the specified mutex is a spin mutex.

## How to use mutex\_owner to check whether the calling thread doesn't have ownership of a mutex

```
fbt::mutex_lock:entry
{
        this->mutex = arg0;
}

fbt::mutex_lock:return
{
        this->owner = mutex_owner((struct mutex *)this->mutex);
}

fbt::mutex_lock:return
/this->owner != curthread/
{
        printf("current thread is not current owner of owned lock\n");
        exit(1);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

