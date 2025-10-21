# Raw FBT Provider

The [fbt provider](../reference/dtrace_providers_fbt.md) consists of
probes that are associated with the entry to and return from most functions
in the Linux kernel.  It does not support tracing synthetic functions,
that is, compiler-generated functions with a . in their name.

In contrast, the `rawfbt` provider implements a variant of the FBT provider
and always uses kprobes.  It does allow tracing of synthetic functions, such
as compiler-generated optimized variants of functions with . suffixes.

You can see the raw FBT probes on your system with:

```
sudo dtrace -lP rawfbt
```

As with the `fbt` provider, there could be tens of thousands of probes,
and effective use requires knowledge of the kernel implementation.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## rawfbt Probes <a id="dt_ref_rawfbtprobes_prov">

The module name of a `rawfbt` probe is `vmlinux` for built-in modules.
The function name is the probed function.
The probe name is either `entry` or `return`.

## rawfbt Probe Arguments <a id="dt_ref_rawfbtargs_prov">

The arguments to `entry` probes are the same as the arguments to the corresponding operating system kernel function.
These arguments can be accessed as `int64_t` values by using the `arg0`, `arg1`, `arg2`, ... variables.

If the function has a return value, the return value is stored in `arg1` of the `return` probe.
If a function doesn't have a return value, `arg1` isn't defined.

There are no typed `args[]` arguments for any `rawfbt` probes.

## rawfbt Stability <a id="dt_ref_rawfbtstab_prov">

The `rawfbt` provider uses DTrace's stability mechanism to describe its stabilities.
These stability values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | Common           |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | ISA              |
| Name      | Evolving       | Evolving       | Common           |
| Arguments | Private        | Private        | ISA              |
