
# dtrace Command Options {#dtrace_command_options}

The `dtrace` command accepts the following options:

```
dtrace [-32 | -64] [-CeFGHhlqSvVwZ] [-b *bufsz*] [-c *cmd*] [-D *name*[=*value*]] 
     [-I *path*] [-L *path*] [-o *output*] [-p *pid*] [-s *script*] [-U *name*] 
     [-x opt[=*val*]] [-X a | c | s | t] 
     [-P *provider* [[*predicate*] *action*]] 
     [-m [*provider*:] *module* [[*predicate*] *action*]] 
     [-f [[*provider*:] *module*:] *function* [[*predicate*] *action*]] 
     [-n [[[*provider*:] *module*:] *function*:] *name* [[*predicate*] *action*]] 
     [-i *probe-id* [[*predicate*] *action*]]
```

where `*predicate*` is any D predicate inside slashes `//` and `*action*` is any D statement list inside braces `{}`, according to the D language syntax.

The arguments accepted by the `-P`, `-m`, `-f`, `-n`, and `-i` options can include an optional D language *predicate* inside slashes `//` and optional D language *action* statement list inside braces `{}`. D program code specified on the command line must be appropriately quoted to avoid interpretation of metacharacters by the shell.

The following options are available:

-   **`-32 | -64`**

    Sets whether to generate 32-bit or 64-bit D programs and ELF files. This option isn't usually required as `dtrace` selects the native data model as the default.

-   **`-b _bufsz_`**

    Set principal trace buffer size \(`*bufsz*`\). The trace buffer size can include any of the size suffixes `k`, `m`, `g`, or `t`. If the buffer space can't be allocated, `dtrace` tries to reduce the buffer size or exit depending on the setting of the `bufresize` property.

-   **`-c _cmd_`**

    Run the specified command `*cmd*` and exit upon its completion. If more than one `-c` option is present on the command line, `dtrace` exits when all commands have exited, reporting the exit status for each child process as it ends. The process-ID of the first command is made available to any D programs specified on the command line or using the `-s` option through the `$target` macro variable.

-   **`-C`**

    Run the C preprocessor \(`cpp`\) over D programs before compiling them. You can pass options to the C preprocessor using the `-D`, `-U`, `-I`, and `-H` options. You can select the degree of C standard conformance if you use the `-X` option. For a description of the set of tokens defined by the D compiler when invoking the C preprocessor, see `-X`.

-   **`-D _name_[=_value_]`**

    Define *name* when invoking cpp \(enabled using the `-C` option\). If you specify the equals sign \(`=`\) and optional *value*, the name is assigned the corresponding value. This option passes the `-D` option to each `cpp` invocation.

-   **`-e`**

    Exit after compiling any requests, but before enabling any probes. You can combine this option with D compiler options. This combination verifies that the programs compile without executing them and enabling the corresponding instrumentation.

-   **`-f [[_provider_:]_module_:]_function_[[_predicate_]_action_]]`**

    Specify function name to trace or list \(`-l` option\). The corresponding argument can include any of the probe description forms `*provider:module:function*`, `*module:function*`, or `*function*`. Unspecified probe description fields are blank and match any probes regardless of the values in those fields. If no qualifiers other than `*function*` are specified in the description, all probes with the corresponding `*function*` are matched. The `-f` argument can be suffixed with an optional D probe clause. You can specify more than one `-f` option on the command line at a time.

-   **`-F`**

    Coalesce trace output by identifying function entry and return. Function entry probe reports are indented and their output is prefixed with `->`. Function return probe reports are unindented and their output is prefixed with `<-`. System call entry probe reports are indented and their output is prefixed with `=>`. System call return probe reports are unindented and their output is prefixed with `<=`.

-   **`-G`**

    Generate an ELF file containing an embedded DTrace program. The DTrace probes specified in the program are saved inside a relocatable ELF object which can be linked into another program. If the `-o` option is present, the ELF file is saved using the path name specified as the argument for this operand. If the `-o` option isn't present and the DTrace program is contained with a file whose name is `*filename*.d`, then the ELF file is saved using the name `*filename*.o`. Otherwise the ELF file is saved using the name `d.out`.

-   **`-H`**

    Print the path names of included files when invoking `cpp` \(enabled using the `-C` option\). This option passes the `-H` option to each `cpp` invocation, causing it to display the list of path names, one for each line, to `stderr`.

-   **`-h`**

    Generate a header file containing macros that correspond to probes in the specified provider definitions. This option can generate a header file that's included by other source files for later use with the `-G` option. If the `-o` option is present, the header file is saved using the path name specified as the argument for that option. If the `-o` option isn't present and the DTrace program is contained with a file whose name is `*filename*.d`, then the header file is saved using the name `*filename*.h`.

-   **`-i _probe-id_ [[_predicate_] _action_]`**

    Specify probe identifier \(`*probe-id*`\) to trace or list \(`-l` option\). You can specify probe IDs using decimal integers as shown by `dtrace` `-l`. The `-i` argument can be suffixed with an optional D probe clause. You can specify more than one `-i` option at a time.

-   **`-I _path_`**

    Add the specified directory `*path*` to the search path for `#include` files when invoking `cpp` \(enabled using the `-C` option\). This option passes the `-I` option to each `cpp` invocation. The specified `*path*` is inserted into the search path ahead of the default directory list.

-   **`-l`**

    List probes instead of enabling them. If the `-l` option is specified, `dtrace` produces a report of the probes matching the descriptions provided using the `-P`, `-m`, `-f`, `-n`, `-i`, and `-s` options. If none of these options are specified, this option lists all probes.

-   **`-L`**

    Add the specified directory `*path*` to the search path for DTrace libraries. DTrace libraries are used to contain common definitions that can be used when writing D programs. The specified `*path*` is added after the default library search path. If it exists, a subdirectory of `*path*` named after the minor version of the running kernel \(for example, 3.8\) is searched immediately before `*path*`. Dependency analysis is performed only within each directory, not across directories.

-   **`-m [[_provider:_] _module:_ [[_predicate_] _action_]]`**

    Specify module name to trace or list \(`-l` option\). The corresponding argument can include any of the probe description forms `*provider:module*` or `*module*`. Unspecified probe description fields are blank and match any probes regardless of the values in those fields. If no qualifiers other than `*module*` are specified in the description, all probes with a corresponding `*module*` are matched. The `-m` argument can be suffixed with an optional D probe clause. More than one `-m` option can be specified on the command line at a time.

-   **`-n [[[_provider:_] _module:_] _function:_] _name_ [[_predicate_] _action_]`**

    Specify probe name to trace or list \(`-l` option\). The corresponding argument can include any of the probe description forms `*provider:module:function:name*`, `*module:function:name*`, `*function:name*`, or `*name*`. Unspecified probe description fields are blank and match any probes regardless of the values in those fields. If no qualifiers other than `*name*` are specified in the description, all probes with a corresponding `*name*` are matched. The `-n` argument can be suffixed with an optional D probe clause. More than one `-n` option can be specified on the command line at a time.

-   **`-o _output_`**

    Specify the `*output*` file for the `-G`, `-h`, and `-l` options, or for the traced data itself. If the `-G` option is present and the `-s` option's argument is of the form `*filename*.d` and `-o` isn't present, the default output file is `*filename*.o`. Otherwise the default output file is `d.out`.

-   **`-p _pid_`**

    Grab the specified process-ID `*pid*`, cache its symbol tables, and exit upon its completion. If more than one `-p` option is present on the command line, `dtrace` exits when all commands have exited, reporting the exit status for each process as it ends. The first process-ID is made available to any D programs specified on the command line or using the `-s` option through the `$target` macro variable.

-   **`-P _provider_`[[_predicate_]`_action_]`**

    Specify provider name to trace or list \(`-l` option\). The remaining probe description fields module, function, and name are blank and match any probes regardless of the values in those fields. The `-P` argument can be suffixed with an optional D probe clause. You can specify more than one `-P` option on the command line at a time.

-   **`-q`**

    Set quiet mode. `dtrace` suppresses messages such as the number of probes matched by the specified options and D programs and doesn't print column headers, the CPU ID, the probe ID, or insert newlines into the output. Only data traced and formatted by D program statements such as `trace()` and `printf()` is displayed to `stdout`.

-   **`-s`**

    Compile the specified D program source file. If the `-e` option is present, the program is compiled but instrumentation isn't enabled. If the `-l` option is present, the program is compiled and the set of probes matched by it, is listed but instrumentation isn't enabled. If none of `-e`, `-l`, or `-G` are present, the instrumentation specified by the D program is enabled and tracing begins.

-   **`-S`**

    Show D compiler intermediate code. The D compiler produces a report of the intermediate code generated for each D program to `stderr`.

-   **`-U _name_`**

    Undefine the specified `*name*` when invoking `cpp` \(enabled using the `-C` option\). This option passes the `-U` option to each `cpp` invocation.

-   **`-v`**

    Set verbose mode. If the `-v` option is specified, `dtrace` produces a program stability report showing the minimum interface stability and dependency level for the specified D programs.

-   **`-V`**

    Report the highest D programming interface for `dtrace`. The version information is printed to `stdout` and the `dtrace` command exits. When used with `-v`, also reports information on the version of the `dtrace` and associated library.

-   **`-w`**

    Permit destructive actions in D programs specified using the `-s`, `-P`, `-m`, `-f`, `-n`, or `-i` options. If the `-w` option isn't specified, `dtrace` doesn't permit the compilation or enabling of a D program that contains destructive actions.

-   **`-x _opt_[_=val_]`**

    Enable or change a DTrace runtime option or D compiler option. Boolean options are enabled by specifying their name. Options with values are set by separating the option name and value with an equals sign \(`=`\). See [DTrace Runtime and Compile-time Options Reference](dtrace_runtime_options.md).

-   **`-X a | c | s | t`**

    Sets the ISO C conformance settings for the preprocessor. The options are:

    -   `a | c | t`: Any of these options sets the conformance to `-std=c99`. This is the default.

    -   `s`: This option sets the conformance to `-traditional-cpp`.

-   **`-Z`**

    Permit probe descriptions that match zero probes. If the `-Z` option isn't specified, `dtrace` reports an error and exits if any probe descriptions specified in D program files \(`-s` option\) or on the command line \(`-P`, `-m`, `-f`, `-n`, or `-i` options\) contain descriptions that don't match any known probes.


**Parent topic:**[DTrace Command Reference](../reference/dtrace_command_reference.md)

