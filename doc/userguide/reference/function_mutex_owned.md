
# mutex\_owned

Checks whether a thread holds the specified kernel mutex.

```
int mutex_owned(vmlinux`struct mutex *)
```

The `mutex_owned` function returns non-zero if the calling thread holds the specified kernel mutex, or zero otherwise.

## How to use mutex\_owned to check whether the calling thread holds a mutex

```
fbt::mutex_lock:entry
{
        this->mutex = arg0;
}

fbt::mutex_lock:return
{
        this->owned = mutex_owned((struct mutex *)this->mutex);
}

fbt::mutex_lock:return
/!this->owned/
{
        printf("mutex_owned() returned 0, expected non-zero\n");
        exit(1);
}

```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

