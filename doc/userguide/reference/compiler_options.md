
# Compile-time Options <a id="dt_compiler_options">

Compile-time options can control how DTrace programs are compiled into eBPF code that's loaded into kernel space.

-   **`aggpercpu`**

    Compile-time option that reports aggregations as usual and on a per-cpu basis. Per-cpu aggregations can also be seen by adding `cpu` as an aggregation key

-   **`amin=<string>`**

    Compile-time option that sets the stability attribute minimum.

-   **`argref`**

    Compile-time option that disables the requirement to use all macro arguments.

-   **`core`**

    Compile-time option that enables core dumping by `dtrace`.

-   **`cpp`**

    Compile-time option that enables `cpp` to preprocess the input file.

-   **`cppargs`**

    Compile-time option that specifies and extra arguments to pass to cpp \(when using -C\).

-   **`cpphdrs`**

    Compile-time option that specifies the `-H` option to `cpp` to print the name of each header file used.

-   **`cpppath=<string>`**

    Compile-time option that specifies the path name of `cpp`.

-   **`ctfpath`**

    Compile-time option that can specify the path of vmlinux.ctfa.

-   **`ctypes=<string>`**

    Compile-time option that specifies Compact Type Format \(CTF\) definitions of all C types used in a program at the end of a D compilation run.

-   **`debug`**

    Compile-time option that enables DTrace debugging mode. This option is the same as setting the environment variable `DTRACE_DEBUG`.

-   **`debugassert`**

    Compile-time option that can enable specific debug modes \[UNTESTED\].

-   **`defaultargs`**

    Compile-time option that allows references to unspecified macro arguments. Use `0` as the value for an unspecified argument.

-   **`define=<string>`**

    Compile-time option that specifies a macro name and optional value in the form *name*\[=*value*\]. This option is the same as running `dtrace` `-D`.

-   **`disasm`**

    Compile-time option to specify requested disassembler listings \(when using -S\).

-   **`droptags`**

    Compile-time option that specifies that drop tags are used.

-   **`dtypes=<string>`**

    Compile-time option that specifies CTF definitions of all D types that are used in a program at the end of a D compilation run.

-   **`empty`**

    Compile-time option that permits compilation of empty D source files.

-   **`errtags`**

    Compile-time option that prefixes default error message with error tags.

-   **`evaltime=[exec|main|postinit|preinit]`**

    Compile-time option that controls when DTrace starts tracing a new process. For dynamically linked binaries, tracing starts:

    -   **exec**: After `exec()`.
    -   **preinit**: After initialization of the dynamic linker to load the binary.
    -   **postinit**: After constructor execution. Default value.
    -   **main**: Before `main()` starts. Same as `postinit`.
    For statically linked binaries, `preinit` is equivalent to `exec`.

    For stripped, statically linked binaries, `postinit` and `main` are equivalent to `preinit`.

-   **`incdir=<string>`**

    Compile-time option that adds an `#include` directory to the preprocessor search path. This option is the same as running `dtrace` `-I`.

-   **`iregs=<scalar>`**

    Compile-time option that sets the size of the DTrace Intermediate Format \(DIF\) integer register set. The default value is `8`.

-   **`kdefs`**

    Compile-time option that prevents unresolved kernel symbols.

-   **`knodefs`**

    Compile-time option that permits unresolved kernel symbols.

-   **`late=[dynamic|static]`**

    Compile-time option that specifies whether to permit references to dynamic translators:

    -   **dynamic**: Allow references to dynamic translators.
    -   **static**: Require translators to be statically defined.
-   **`lazyload=<true|false>`**

    Compile-time option that specifies lazy loading for the DTrace Object Format \(DOF\) rather than active loading.

-   **`ldpath=<string>`**

    Compile-time option that specifies the path of the dynamic linker loader \(`ld`\).

-   **`libdir=<string>`**

    Compile-time option that adds a library directory to the library search path.

-   **`linkmode=[dynamic|kernel|static]`**

    Compile-time option that specifies the symbol linking mode used by the assembler when processing external symbol references:

    -   **dynamic**: All symbols are treated as dynamic.
    -   **kernel**: Kernel symbols are treated as static and user symbols are treated as dynamic.
    -   **static**: All symbols are treated as static.
-   **`linknommap`**

    Compile-time option to disable use of MMAP-based libelf support when linking USDT objects.

-   **`linktype=[dof|elf]`**

    Compile-time option that specifies the output file type:

    -   **dof**: Produce a standalone DOF file.
    -   **elf**: Produce an ELF file that contains DOF.
-   **`modpath=<string>`**

    Compile-time option that specifies the module path. The default path is `/lib/modules/` *version*.

-   **`nolibs`**

    Compile-time option that prevents processing D system libraries.

-   **`pgmax=<scalar>`**

    Compile-time option that sets a limit on the number of threads that DTrace can grab for tracing. The default value is `8`.

-   **`preallocate=<scalar>`**

    Compile-time option that sets the amount of memory to preallocate.

-   **`procfspath=<string>`**

    Compile-time option that sets the path to the `procfs` file system. The default path is `/proc`.

-   **`pspec`**

    Compile-time option that enables interpretation of ambiguous specifiers as probe names.

-   **`stdc=[a|c|s|t]`**

    Compile-time option that specifies ISO C conformance settings for the preprocessor when invoking `cpp` with the `-C` option.

    The `a`, `c`, and `t` settings include the`-std=gnu99` option \(conformance with 1999 C standard including GNU extensions\).

    The `s` setting includes the `-traditional-cpp` option \(conformance with K&amp;R C\).

-   **`strip`**

    Compile-time option that strips non-loadable sections from the program.

-   **`syslibdir=<string>`**

    Compile-time option that sets the path name of system libraries.

-   **`tree=<scalar>`**

    Compile-time option that sets the value of the DTrace tree dump bitmap.

-   **`tregs=<scalar>`**

    Compile-time option that sets the size of the DIF tuple register set. The default value is `8`.

-   **`udefs`**

    Compile-time option that prevents unresolved user symbols.

-   **`undef=<string>`**

    Compile-time option that undefines a symbol when invoking the preprocessor. This option is the same as running `dtrace` `-U`.

-   **`unodefs`**

    Compile-time option that permits unresolved user symbols.

-   **`useruid`**

    Compile-time option to use first UID that isn't in the system range .

-   **`verbose`**

    Compile-time option that enables DIF verbose mode, which shows each compiled DIF object \(DIFO\).

-   **`version=<string>`**

    Compile-time option that requests a specific version of the DTrace library.

-   **`zdefs`**

    Compile-time option that permits probe definitions that match zero probes.


**Parent topic:**[DTrace Runtime and Compile-time Options Reference](../reference/dtrace_runtime_options.md)

