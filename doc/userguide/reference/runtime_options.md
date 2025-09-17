
# Runtime Options

Runtime options can control how the DTrace utility behaves.

-   **`aggsize=<size>`**

    Runtime option that sets the buffer size for aggregation.

-   **`bpflog=<size>`**

    Runtime option that forces reporting of the BPF verifier log \(even if verification was successful\).

-   **`bpflogsize`**

    Runtime option that sets the maximum size of the BPF verifier log.

-   **`bufsize=<size>`**

    Runtime option that sets the principal buffer size. The default buffer size is set to 4 MB. This option is the same as running `dtrace` `-b`.

-   **`cleanrate=<time>`**

    Runtime option that sets the cleaning rate.

-   **`cpu=<scalar>`**

    Runtime option that restricts tracing to a particular CPU.

-   **`destructive`**

    Runtime option that permits destructive functions to run. This option is the same as running `dtrace` `-w`.

-   **`dynvarsize=<size>`**

    Runtime option that sets dynamic variable space size.

-   **`lockmem`**

    Runtime option that sets the locked pages limit. This is set to `unlimited` by default.

-   **`maxframes=<scalar>`**

    Runtime option that sets the maximum number of stack frames reported by the kernel.

-   **`noresolve`**

    Runtime option that disables automatic resolving of userspace symbols.

-   **`nspec=<scalar>`**

    Runtime option that sets the number of speculations.

-   **`pcapsize=<size>`**

    Runtime option that sets the maximum packet data capture size.

-   **`scratchsize=<size>`**

    Runtime option that sets the maximum DTrace scratch memory size. Some functions in DTrace require that *scratch memory*, is made available. For example, when you allocate memory in a program by using the `alloca()` function, scratch memory is used for this purpose. Scratch memory is only valid while a clause is being processed and is released when the clause has finished being processed. If there isn't enough scratch memory, a function in a DTrace script can return an error and any remaining processing of the clause might fail. The default value is 256 bytes.

-   **`specsize=<size>`**

    Runtime option that sets the speculation buffer size.

-   **`stackframes=<scalar>`**

    Runtime option that sets the number of stack frames. The default value is 20.

-   **`statusrate=<time>`**

    Runtime option that sets the rate of status checking.

-   **`strsize=<size>`**

    Runtime option that sets the string size. The default value is 256.

-   **`ustackframes=<scalar>`**

    Runtime option that sets the number of user-land stack frames. The default value is 100.


**Parent topic:**[DTrace Runtime and Compile-time Options Reference](../reference/dtrace_runtime_options.md)

