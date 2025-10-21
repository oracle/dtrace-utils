
# Pid Provider <a id="dt_ref_pid_prov">

The `pid` provider traces a user process, both function `entry` and `return`, and an arbitrary instruction.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## pid Probes <a id="dt_ref_pidprobes_prov">

A probe is fully specified by naming its provider, module, function, and name.

A `pid` provider name is `pid*$pid*`, where `*$pid*` is the process ID \(pid\) of the process you're interested in. The process ID must be specified \(no wild cards\), but symbolic values can be used. For example:

```
sudo dtrace -lP pid
```

The previous command doesn't find any probes because no process ID was specified. Instead, use commands such as:

```
sudo dtrace -ln 'pid1234567:libc::entry'
sudo dtrace -p 1234567 -ln 'pid$target:libc::entry'
sudo dtrace -c ./a.out -n 'pid$target:libc::entry { trace("hi"); }'
sudo dtrace -n 'pid$1:libc::entry { trace("hi"); }'
```

In this example, `1234567` is a fictious pid value that should be replaced with a pid value appropriate to the system, `$target` is a macro that expands to the pid of the target command specified by `-p` or `-c`, and `$1` is the command line argument to `dtrace`. In this example, the following is displayed:

```
1234567
```

The module name is the module within the executable. Special cases include the load object of the executable, which might be referred to as `a.out`, and a shared library that can be referred to by:

-   A full pathname, for example, `/usr/lib/libc.so.1`.

-   A basename, for example, `libc.so.1`.

-   An initial basename match up to a `.` suffix, for example, `libc.so` or `libc`.


The function name is typically the name of the function where the probe is located. If a function is inlined by the compiler, it's not available for pid tracing.

There is a special function `-`. In this case, the module name must be blank or refer to the `a.out` module. Further, the probe name must be an absolute hexadecimal offset to some instruction within the `a.out` module.

The probe name is one of:

-   `entry`: refers to the entry to the associated function.

-   `return`: refers to a return from the associated function. In traditional implementations of DTrace, the probe fired at some return instruction. In the current implementation, a `return` is implemented with a `uretprobe`, firing in the caller function.

-   An instruction offset. The hexadecimal offset, without a leading `0x`, is relative to the named function, but it's an absolute offset when the function name is `-`.


## pid Probe Arguments <a id="dt_ref_pidargs_prov">

For `entry` probes, the probe arguments are the same arguments as those of the probed function.

For `return` probes, `arg1` is the return value of the probed function.

For offset probes, there are no probe arguments.

## pid Examples <a id="dt_ref_pidexamples_prov">

Consider the following program, named `main.c`, that calls a function `foo()`:

```
 int foo(int i, int j) {
     return (i + j) - 6666;
 }

 int main(int c, char **v) {
     return foo(1234, 8765) != 3333;
 }
```

The arguments to `foo()` are `1234` and `8765`, while the return value is `3333`.

Compile the program:

```
gcc main.c
```

We create a D script named `D1.d`:

```
 pid$target:a.out:foo:entry,
 pid$target:a.out:foo:return
 {
     printf("%x %s:%s\n", uregs[R_PC], probefunc, probename);
 }

 pid$target:a.out:foo:entry
 {
     printf("entry args: %d %d\n", arg0, arg1);
 }

 pid$target:a.out:foo:return
 {
     printf("return arg: %d\n", arg1);
 }
```

Run the D script:

```
sudo dtrace -c ./a.out -qs D1.d
```

The output looks similar to:

```
 401106 foo:entry
 entry args: 1234 8765
 40113d foo:return
 return arg: 3333
```

On `foo()` `entry` and `return`, we print the PC from the target thread's saved user-mode register values at probe firing time, along with the probe function and name. In addition, we print the entry probe's two arguments and the return probe's `arg1`. We see that the `foo()` entry arguments are `1234` and `8765`, and the return value is `3333`, as expected.

To understand the PCs, run `objdump`:

```
 objdump -d a.out
```

The output looks similar to:

```
 0000000000401106 <foo>:
   401106:    55                       push   %rbp
   401107:    48 89 e5                 mov    %rsp,%rbp
   40110a:    89 7d fc                 mov    %edi,-0x4(%rbp)
   [...]

 000000000040111f <main>:
   40111f:    55                       push   %rbp
   [...]
   401138:    e8 c9 ff ff ff           callq  401106 <foo>
   40113d:    83 f0 01                 xor    $0x1,%eax
   [...]
```

Much of the output has been suppressed, but we see the `foo()` `entry` PC is `0x401106`, as reported by our D script. The return PC is `0x40113d`, which is the PC immediately after the `foo()` call.

**Note:**

In other versions of DTrace, the return PC is for the return instruction in the called function. On Linux, we use a return `uprobe` \(a `uretprobe`\) which returns an instruction in the caller, as we saw.

Finally, we illustrate how to probe on a specific instruction. We select the third instruction in `foo()`, PC `0x40110a`. This is at a relative offset of 4 bytes from the start of `foo()`. This D script is named D2.d:

```
 pid$target:a.out:foo:4,
 pid$target:a.out:-:40110a
 {
     printf("%x %s:%s\n", uregs[R_PC], probefunc, probename);
 }
```

Run the D script:

```
sudo dtrace -c ./a.out -qs D2.d
```

The output looks similar to:

```
 40110a foo:4
 40110a -:40110a
```

We probe on the chosen instruction, using both a relative offset `foo:4`, and an absolute offset `-:40110a`. Both probes fire, both reporting the same PC `0x40110a`.

## pid Stability <a id="dt_ref_pidstab_prov">

The `pid` provider uses DTrace's stability mechanism to describe its stabilities. These values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | ISA              |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Evolving       | Evolving       | ISA              |
| Arguments | Private        | Private        | Unknown          |
