
# mutex\_type\_spin

Returns a non zero value if a specified kernel mutex is a spin mutex.

```
mutex_type_spin(int(vmlinux`struct mutex *))
```

The `mutex_type_spin` function returns a non zero value if a specified kernel mutex is a spin mutex. All mutexes in the Linux kernel are adaptive, so the `mutex_type_spin` function always returns `0`.

## How to use mutex\_type\_spin to check whether a mutex is a spin mutex

Because all mutexes on Linux are adaptive, the final clause in this program is never processed.

```
fbt::mutex_lock:entry
{
        this->mutex = arg0;
}

fbt::mutex_lock:return
{
        this->spin = mutex_type_spin((struct mutex *)this->mutex);
}

fbt::mutex_lock:return
/this->spin/
{
        printf("mutex_type_spin returned non-zero, expected 0\n");
        exit(1);
}

```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

