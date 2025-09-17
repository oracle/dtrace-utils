
# setopt

Dynamically sets DTrace compiler or runtime options.

```
void setopt(const char *[, const char *])
```

The `setopt` function is a special function that can be used to specify a DTrace runtime or compiler option dynamically. See [DTrace Runtime and Compile-time Options Reference](dtrace_runtime_options.md) for more information.

## How to use setopt to set compiler or runtime options inside a program

```
setopt("quiet");
setopt("bufsize", "50m");
setopt("aggrate", "2hz");
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

