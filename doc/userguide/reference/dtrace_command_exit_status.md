
# dtrace Command Exit Status {#dtrace_command_exit_status}

The following exit values are returned by the `dtrace` command:

-   **`0`**

    Indicates that the specified requests were completed successfully. For D program requests, the `0` exit status indicates that programs were successfully compiled, probes were successfully enabled, or an anonymous state was successfully retrieved. The `dtrace` command returns `0` even if the specified tracing requests meet errors or drops.


-   **`1`**

    Indicates that a fatal error occurred. For D program requests, the `1` exit status indicates that program compilation failed or that the specified request couldn't be satisfied.


-   **`2`**

    Indicates that invalid command line options or arguments were specified.


**Parent topic:**[DTrace Command Reference](../reference/dtrace_command_reference.md)

