
# DTrace Runtime and Compile-time Options Reference <a id="dt_runtime_options">

DTrace uses reasonable default values and flexible default policies for runtime configuration. Tuning mechanisms in the form of DTrace compiler or runtime option can change the default behavior of the `dtrace` utility. You can find more information about the `dtrace` utility and various command line options in the `dtrace(8)` manual page.

Options that can be specified when running the `dtrace` utility can be categorized into three types:

-   [Compile-time Options](compiler_options.md): affect the compilation process but might also affect runtime behavior.

-   [Runtime Options](runtime_options.md): affect the runtime behavior of DTrace but which are often set at compile time.

-   [Dynamic Runtime Options](dynamic_runtime_options.md): affect the runtime behavior of DTrace but which can be changed while tracing,
    by using the [`setopt`](function_setopt.md) function.


-   **[Setting DTrace Compile-time and Runtime Options](../reference/setting_dtrace_compiler_and_runtime_options.md)**  
You can tune DTrace by setting or enabling a selection of runtime or compiler options. You can set options by either using the `-x` command line switch when running the `dtrace` command, or by specifying `pragma` lines in D programs. If an option takes a value, follow the option name with an equal sign \(`=`\) and the option value.
-   **[Compile-time Options](../reference/compiler_options.md)**  
Compile-time options can control how DTrace programs are compiled into eBPF code that's loaded into kernel space.
-   **[Runtime Options](../reference/runtime_options.md)**  
Runtime options can control how the DTrace utility behaves.
-   **[Dynamic Runtime Options](../reference/dynamic_runtime_options.md)**  
Dynamic runtime options are specific to D programs themselves and are likely to change depending on program functionality and requirements.
