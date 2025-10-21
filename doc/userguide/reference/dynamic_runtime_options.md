
# Dynamic Runtime Options <a id="dt_dynamic_runtime_options">

Dynamic runtime options are specific to D programs themselves and are likely to change depending on program functionality and requirements.

-   **`aggrate=<time>`**

    Dynamic runtime option that sets the amount of time between aggregation readings.

-   **`aggsortkey=<true|false>`**

    Dynamic runtime option that sorts aggregations by key.

-   **`aggsortkeypos=<scalar>`**

    Dynamic runtime option that sets the position, or number, of the aggregation key on which to sort.

-   **`aggsortpos=<scalar>`**

    Dynamic runtime option that sets the position, or number, of the aggregation variable on which to sort

-   **`aggsortrev=<true|false>`**

    Dynamic runtime option that sorts aggregations in reverse order.

-   **`flowindent`**

    Dynamic runtime option that controls indentation.

    Indent function entry and prefix with `->`.

    Unindent function return and prefix with `<-`.

    Indent system call entry and prefix with `=>`.

    Unindent system call return and prefix with `<=`.

    This option is the same as running `dtrace` `-F`.

-   **`quiet`**

    Dynamic runtime option that restricts output to explicitly traced data. This option is the same as running `dtrace -q`.

-   **`quietresize`**

    Dynamic runtime option that suppresses buffer-resize messages.

-   **`rawbytes`**

    Dynamic runtime option that prints `trace` output in hexadecimal.

-   **`stackindent=<scalar>`**

    Dynamic runtime option that sets the number of white space characters to use when indenting `stack` and `ustack` output. The default value is 14.

-   **`switchrate=<time>`**

    Dynamic runtime option that sets the rate at which the buffer is read. You can increase the rate to help prevent data drops, or consider increasing the size of the principal buffer with the `bufsize` option.


**Parent topic:**[DTrace Runtime and Compile-time Options Reference](../reference/dtrace_runtime_options.md)

