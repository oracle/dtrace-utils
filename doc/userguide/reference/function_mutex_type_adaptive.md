
# mutex\_type\_adaptive

Returns a non zero value if a specified kernel mutex is adaptive.

```
int mutex_type_adaptive(vmlinux`struct mutex *)
```

The `mutex_type_adaptive` function returns a non zero value if a specified kernel mutex is adaptive. All mutexes in the Linux kernel are adaptive, so the `mutex_type_adaptive` function always returns `1`.

## How to use mutex\_type\_adaptive to check whether a mutex isn't adaptive

Because all mutexes on Linux are adaptive, the final clause in this program is never processed.

```
fbt::mutex_lock:entry
{
        this->mutex = arg0;
}

fbt::mutex_lock:return
{
        this->adaptive = mutex_type_adaptive((struct mutex *)this->mutex);
}

fbt::mutex_lock:return
/!this->adaptive/
{
        printf("mutex_type_adaptive returned 0, expected non-zero\n");
        exit(1);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

