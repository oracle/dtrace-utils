
# Stapsdt Provider

The `stapsdt` provider allows DTrace to read ELF notes for binaries, using
`/proc/` maps associated with processes, and parse them to retrieve `uprobe`
addresses and argument-related information to create the associated `uprobe`.
That is, the provider allows DTrace to trace programs and libraries that
contain static probes that were added via `stapsdt` ELF notes.

Note that `stapsdt` probes do not dynamically register themselves with DTrace,
making them less powerful than DTrace-based USDT probes.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## stapsdt Probes <a id="dt_ref_stapsdtprobes_prov">

You will typically not insert `stapsdt` probes into source code yourself,
since DTrace-based USDT probes are preferable, dynamically registering
themselves with DTrace.  However, you might well want to take
advantage of static probes already inserted into the ELF notes of binaries
and libraries by other developers.  For example, probes for DTrace
and SystemTap were added to Python as far back as Python 3.6 provided
Python is built with [`--with-dtrace`](https://docs.python.org/3/howto/instrumentation.html).

Nonetheless, you can insert your own `stapsdt` probes into application or
library source code as follows:

```
#define _SDT_HAS_SEMAPHORES 1
#include <sys/sdt.h>

unsigned short myprovider_myprobe_semaphore = 0;

        if (myprovider_myprobe_semaphore) {
            STAP_PROBEn(myprovider, myprobe, ...);
        }
```

The `...` indicate the probe arguments;  note that `STAP_PROBEn` should be
replaced by `STAP_PROBE`, `STAP_PROBE1`, `STAP_PROBE2`, `STAP_PROBE3`, etc.,
depending on how many probe arguments there are.

The *semaphore* portions are only needed if you want to test at run time
whether the probe is enabled;  this can reduce run-time overheads for cases
where preparing probe arguments is expensive and yet the probe is not enabled
and the arguments unneeded.

(Note:  the sempahore counting is handled by the kernel on probe enable and
disable.  We simply need to ensure the semaphore is defined as above, and
DTrace will pass semaphore details to the kernel for reference counting.)

The `stapsdt` provider also supports probes created dynamically via `libstapsdt`.
See [https://github.com/linux-usdt/libstapsdt](https://github.com/linux-usdt/libstapsdt).

In your D scripts, the provider name must have the pid for your process
appended.  For the provider shown above, you might have `myprovider12345` for
pid 12345.  No wildcard may be used, but a symbolic value like `$target` may.

In your D scripts, the `stapsdt` module name may be `a.out`, just as with
the [`pid` provider](dtrace_providers_pid.md#dt_ref_pidprobes_prov).

In probe names, two consecutive underscores \(`__`\) become a dash \(`-`\)
just as with USDT probe names.  For example, if a probe is defined in source
code as `my__probe`, then refer to it as `my-probe` in your D scripts.

## stapsdt Examples <a id="dt_ref_stapsdtexamples_prov">

For example, consider this sample source code, which could represent the instrumentation one
might place in application or library source code:

```
#include <stdio.h>
#define _SDT_HAS_SEMAPHORES 1
#include <sys/sdt.h>

unsigned short myprovider_myprobe_semaphore = 0;

int
main(int argc, char **argv)
{
    if (myprovider_myprobe_semaphore) {
        printf("the probe is enabled\n");
        [... do something expensive to prepare probe arguments? ...]
        STAP_PROBE3(myprovider, myprobe, argc, argv[0], 18);
    } else
        printf("the probe is not enabled\n");

    return 0;
}
```

Assuming it is called `test.c`, compile with `gcc test.c`.

We can then run `dtrace -l` to list the available probes,
assuming we specify the pid-specific provider (in this case using
the `$target` symbolic name):

```
$ sudo /usr/sbin/dtrace -c ./a.out -lvP 'myprovider$target'
   ID   PROVIDER            MODULE          FUNCTION NAME
 5067 myprovider2623420     a.out               main myprobe
    [...]
        Argument Types
                args[0]: int32_t
                args[1]: uint64_t
                args[2]: int32_t
```

Since we list the arguments with `-v`, we get verbose information on the
argument types:  the first and last arguments are 4-byte integers, while
the middle argument is 8 bytes (a `char *`).

We can run the program without `dtrace` to confirm that the probe is
not enabled by default:

```
$ ./a.out
the probe is not enabled
```

Next, we run with `dtrace` and see that the probe *is* enabled and
that the probe arguments are `argc, argv[0], 18`, as indicated in
the `test.c` source code:

```
$ sudo /usr/sbin/dtrace -c ./a.out -qn '
myprovider$target:::myprobe
{
    printf("%li, %s, %li\n", arg0, copyinstr(arg1), arg2);
}'
the probe is enabled
1, ./a.out, 18
```

Adventurous readers can use `readelf` to explore the ELF notes
more directly:

```
$ readelf --notes a.out
[...]
Displaying notes found in: .note.stapsdt
  Owner         Data size       Description
  stapsdt       0x00000045      NT_STAPSDT (SystemTap probe descriptors)
    Provider: myprovider
    Name: myprobe
    Location: 0x000000000040113c, Base: 0x000000000040203e, Semaphore: 0x0000000000404026
    Arguments: -4@-4(%rbp) 8@%rax -4@$18
```

Notice that the instrumented source code includes the header file
`<sys/sdt.h>`.  On Oracle Linux, this file is installed
by the RPM package `systemtap-sdt-devel`, but the package also
installs `/usr/bin/dtrace`.  So be sure to have `/usr/sbin`
in front of `/usr/bin` in your path, or explicitly specify
`/usr/sbin/dtrace` whenever you want to use `dtrace`.

## stapsdt Stability <a id="dt_ref_stapsdtstab_prov">

The `stapsdt` provider uses DTrace's stability mechanism to describe its stabilities.
These values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | ISA              |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Evolving       | Evolving       | ISA              |
| Arguments | Private        | Private        | Unknown          |
