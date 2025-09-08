
# Setting DTrace Compile-time and Runtime Options {#dt_runtime_option_description}

You can tune DTrace by setting or enabling a selection of runtime or compiler options. You can set options by either using the `-x` command line switch when running the `dtrace` command, or by specifying `pragma` lines in D programs. If an option takes a value, follow the option name with an equal sign \(`=`\) and the option value.

## Value Suffixes

Use the following optional suffixes for values that denote size or time:

-   `k` or `K`: kilobytes

-   `m` or `M`: megabytes

-   `g` or `G`: gigabytes

-   `t` or `T`: terabytes

-   `ns` or `nsec`: nanoseconds

-   `us` or `usec`: microseconds

-   `ms` or `msec`: milliseconds

-   `s` or `sec`: seconds

-   `m` or `min`: minutes

-   `h` or `hour`: hours

-   `d` or `day`: days

-   `hz`: number per second


## Enabling Options Using the DTrace Utility

The `dtrace` command accepts option settings on the command line by using the `-x` switch, for example:

```
sudo dtrace -x nspec=4 -x bufsize=2g \
-x switchrate=10hz -x aggrate=100us -x bufresize=manual
```

## DTrace Pragma Lines To Enable Options in a D Program

You can set options in a D program by using `#pragma D` followed by the string `option` and the option name and value. The following are examples of valid option settings:

```
#pragma D option nspec=4
#pragma D option bufsize=2g
#pragma D option switchrate=10hz
#pragma D option aggrate=100us
#pragma D option bufresize=manual
```

**Parent topic:**[DTrace Runtime and Compile-time Options Reference](../reference/dtrace_runtime_options.md)

