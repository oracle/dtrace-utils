
# USDT Provider {#dt_ref_usdt_prov}

Use the USDT provider, for user space statically defined tracing, to instrument user space code with probes that are meaningful for an application.

For example, if an application has `put` and `get` operations, you can insert `put` and `get` instrumentation points in the source code, even if each operation is implemented on several code paths. A DTrace user could then enable such probes to trace activity, even without knowing how those operations are implemented in the source code. As usual, there are negligible performance impacts for DTrace probes when the probes aren't enabled. USDT probes can also appear in shared libraries.

USDT is for user space processes. For kernel modules, statically defined tracing is handled by the related SDT mechanism.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## Defining USDT Providers and Probes {#dt_ref_usdtprobes_prov}

Define USDT providers and probes in a `.d` file that you add to the source code. For example, a file `myproviders.d` contains:

```
provider myprov
{
  probe my__put(int, int);
  probe my__get();
};
```

In this example, the provider name is `myprov`, but, as with the `pid` provider, DTrace users must append the process ID \(pid\) of the process or processes that interest them. In contrast to the `pid` provider, the USDT provider descriptions can use wildcards for the pids. For example, specifying `myprov1234` traces this provider's probes only for process ID `1234`. In contrast, `myprov*` traces this provider's probes for all processes that have been appropriately instrumented, even processes that haven't yet started. Or, as with the `pid` provider, you can use a symbolic pid, such as `myprov$target` for the process started with the `-c` option.

The provider definition lists its probes, along with any probe arguments. The D compiler converts two consecutive underscores \(`__`\) to a dash \(`-`\) in the probe name.

This following example runs command `./a.out`, then traces the USDT probe `my-put` on that one process, displaying the two arguments to that probe.

```
sudo dtrace -c ./a.out -n 'myprov$target:::my-put { printf("put %d %d\n", arg0, arg1); }'
```

The following example traces all processes with `myprov` probes, even if they haven't yet started. This example uses the `-Z` option of `dtrace` in case zero processes match at the time `dtrace` is started.

```
sudo dtrace -Z -n 'myprov*:::my-put { printf("put %d %d\n", arg0, arg1); }'
```

## Adding USDT Probes to Application Code {#dt_ref_usdt_probe_add_prov}

Consider this C code `func.c`:

```
#include "myproviders.h"

void foo(void)
{
    ...
    if (MYPROV_MY_PUT_ENABLED()) {
        int arg0, arg1;

        arg0 = bar(1111);
        arg1 = bar(2222);
        MYPROV_MY_PUT(arg0, arg1);
    }
    ...
    MYPROV_MY_GET();
    ...
}
```

This example includes a header file that's automatically generated. The name of this header file is derived from the file name that defines the macros to access the probes. It defines macros that provide access to the USDT probes.

You can place probes in the code, referring to the probes by using the macros, which concatenate provider, and probe names, and converting to uppercase. In this example, the macros are `MYPROV_MY_PUT()` and `MYPROV_MY_GET()`.

An optional optimization is to test if a probe is enabled. While the computational overhead of a disabled DTrace probe is often similar to a few no-op instructions, setting up probe arguments can be expensive. In this example, `bar(1111)` and `bar(2222)`might be costly function calls. Therefore, for each probe, DTrace also supplies an `is-enabled` macro, named by appending `_ENABLED`. In the example, `MYPROV_MY_PUT_ENABLED()` for the `my-put` probe, to help minimize the cost of any work associated with disabled probes.

## Building Applications With USDT Probes {#dt_ref_usdt_build_prov}

The `dtrace` command becomes part of the build procedure, which can be thought of in four parts:

1.  Generate the header file that defines the macros to access the probes. For example:

    ```
    dtrace -h -s myproviders.d
    ```

    The previous command produces the `myproviders.h` header file. While `dtrace` requires root privileges for runtime tracing, generating the header file doesn't have this requirement.

2.  Compile the source code, which includes the `dtrace` generated header file based on the provider and probe definitions. For example, for several source files:

    ```
    gcc -c func1.c
    gcc -c func2.c
    gcc -c func3.c
    ```

3.  Post process each object file using `dtrace`. For example:

    ```
    dtrace -G -s myproviders.d func1.o func2.o func3.o
    ```

    Again, `dtrace` doesn't require root privileges for this step. This step also generates the object file `myproviders.o` from `myproviders.d` and the other object files, linking provider and probe definitions with a user application.

4.  Link the final executable. The `-Wl,--export-dynamic` link options to `gcc` are required for symbol lookup in a stripped executable at runtime, for example, when you use the D function `ustack()`. For example:

    ```
    gcc -Wl,--export-dynamic,--strip-all myproviders.o func1.o func2.o func3.o
    ```


## USDT Examples {#dt_ref_usdtexamples_prov}

1.  Create a file `myproviders.d` that contains:

    ```
    provider myprov
    {
        probe my__put(int, int);
        probe my__get();
    };          
    ```

2.  Create a C program `func.c` that contains:

    ```
    #include <stdio.h>
    #include <unistd.h>
    #include "myproviders.h"
    
    int bar(int in)
    {
        printf("bar evaluates %d\n", in);
        return 3 * in;
    }
    
    void foo(void)
    {
        if (MYPROV_MY_PUT_ENABLED()) {
            int arg0, arg1;
            arg0 = bar(1111);
            arg1 = bar(2222);
            MYPROV_MY_PUT(arg0, arg1);
        }
        MYPROV_MY_GET();
    }
    
    int main(int c, char **v)
    {
        while (1) {
            usleep(1000 * 1000);
            foo();
        }
    
        return 0;
    }
    ```

3.  Build the application using:

    ```
    dtrace -h -s myproviders.d
    gcc -c func.c
    dtrace -G -s myproviders.d func.o
    gcc -Wl,--export-dynamic,--strip-all myproviders.o func.o
    ```

4.  You could run this program in several ways. For example:

    -   You could run the tracing program using:

        ```
        sudo dtrace -c ./a.out -q -n '
            myprov$target:::my-put { printf("put %d %d\n", arg0, arg1); }
            myprov$target:::my-get { printf("get\n"); }
            tick-5sec {exit(0)}'
        ```

        This first example, runs the `a.out` command with the `-c` option. The `-q` quiet option suppresses extraneous output. The D script is on the command line, and it prints both args for the `put` probe and reports the `get` probe. The example refers symbolically to `$target`, the pid of the target command is specified with `-c`.

        After five seconds, the `dtrace` job finishes, ending the target command.

    -   Or, you could run this example using:

        ```
        ./a.out &
        pid=$!
        sudo dtrace -q -n '
                 myprov'$pid':::my-put { printf("put %d %d\n", arg0, arg1); }
                 myprov'$pid':::my-get { printf("get\n"); }
                 tick-5sec {exit(0)}'
        kill $pid
        ```

        In this example, the command is already running with process ID `$pid`. We refer to the specific numerical pid that interests us. The `dtrace` command doesn't, in this case, end the process of interest. We handle that separately.

    -   One more possibility is to run the program using:

        ```
        sudo dtrace -Z -q -n '
            myprov*:::my-put { printf("put %d %d\n", arg0, arg1); }
            myprov*:::my-get { printf("get\n"); }
            tick-10sec {exit(0)}' &
        ./a.out &
        ```

        In this example, the `-Z` option allows for zero probe matches at first. The probes match later, when a USDT process has started. After a short delay, a USDT process is started. At some point, the USDT process is ended.

    In each of these cases, the output is printed after one second, and looks similar to:

    ```
    ...
    bar evaluates 1111
    bar evaluates 2222
    put 3333 6666
    get
    ...
    ```

    The D script can omit the `put` probe using:

    ```
    sudo dtrace -c ./a.out -q -n '
        myprov$target:::my-get { printf("get\n"); }
        tick-5sec {exit(0)}'
    ```

    In this case, not only does the `put` probe not fire, but also the `is-enabled` conditional `MYPROV_MY_PUT_ENABLED()` is false. Therefore, the `bar()` function isn't called. The only output displayed each second is:

    ```
    get
    ```


