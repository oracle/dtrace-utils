
# DTrace Function Reference

You use D function calls to invoke different kinds of services that DTrace provides.

Functions can be grouped according to their general use case and might appear in more than one grouping:

-   **Data Recording Functions**

    Data recording functions record data to a DTrace buffer. These are the most common functions and the default DTrace function belongs to this category. By default, data recording functions record data to the *principal buffer*, but can also be directed to record data into a *speculative buffer*.

    Data recording functions include:

    -   [Default Action](dtrace-ref-DefaultAction.md): The default action applies when DTrace encounters an empty clause for a probe. The default action is to trace the enabled probe identifier \(EPID\).

    -   `[printa](function_printa.md)`: Displays and controls the formatting of an aggregation

    -   `[printf](function_printf.md)`: Displays and controls the formatting of a string.

    -   `[trace](function_trace.md)`: Traces the result of an expression to the directed buffer.

    -   `[tracemem](function_tracemem.md)`: Copies the specified number of bytes of data from an address in memory to the current buffer.

-   **Aggregation Functions**

    Aggregation functions provide calculated information about sets of DTrace data stored in aggregations.

    The following functions are aggregation functions:

    -   `[avg](aggregation_avg.md)`: Stores the arithmetic average of the specified expressions in an aggregation.

    -   `[count](aggregation_count.md)`: Stores an incremented count value in an aggregation.

    -   `[max](aggregation_max.md)`: Stores the largest value among the specified expressions in an aggregation.

    -   `[min](aggregation_min.md)`: Stores the smallest value among the specified expressions in an aggregation.

    -   `[sum](aggregation_sum.md)`: Stores the total value of the specified expression in an aggregation.

    -   `[stddev](aggregation_stddev.md)`: Stores the standard deviation of the specified expressions in an aggregation.

    -   `[quantize](aggregation_quantize.md)`: Stores a power-of-two frequency distribution of the values of the specified expressions in an aggregation. An optional increment can be specified.

    -   `[lquantize](aggregation_lquantize.md)`: Stores the linear frequency distribution of the values of the specified expressions, sized by the specified range, in an aggregation.

    -   `[llquantize](aggregation_llquantize.md)`: Stores the log-linear frequency distribution in an aggregation.

    The following functions aren't aggregating functions but work on aggregations:

    -   `[clear](function_clear.md)`: Clears the values from an aggregation while retaining aggregation keys.

    -   `[denormalize](function_denormalize.md)`: Removes the normalization that's applied to a specified aggregation.

    -   `[normalize](function_normalize.md)`: Divides an aggregation value by a specified normalization factor.

    -   `[printa](function_printa.md)`: Displays and controls the formatting of an aggregation

-   **Speculation Functions**

    Speculation functions create or operate on speculative buffers. Speculation is used to trace quantities into speculation buffers that can either be committed to the primary buffer or discarded at a later point, when other important information is known.

    The following functions are speculation functions:

    -   `[speculation](function_speculation.md)`: Creates a speculative trace buffer and returns its ID.

    -   `[speculate](function_speculate.md)`: A special function that causes DTrace to switch to using a speculation buffer identified by the specified ID for the remainder of a clause.

    -   `[commit](function_commit.md)`: Commits the speculative buffer, specified by ID, to the principal buffer.

    -   `[discard](function_discard.md)`: Discards a speculative buffer specified by the provided speculation ID.

-   **String Manipulation Functions**

    String manipulation functions are typical in most programming languages and are used to perform common functional operations on strings. Many functions have analogs in the system library calls described in section 3 of the Linux manual pages. You can often find out more about these functions by examining the corresponding manual page. For example:

    ```
    man 3 strchr
    ```

    Several of these functions require temporary buffers, which persist only for duration of the clause. Preallocated scratch memory is used for such buffers.

    The following string manipulation functions are available:

    -   `[index](function_index.md)`: Finds the first occurrence of a substring within a string.

    -   `[rindex](function_rindex.md)`: Finds the last occurrence of a specific substring within a string.

    -   `[lltostr](function_lltostr.md)`: Converts an unsigned 64-bit integer to a string.

    -   `[strchr](function_strchr.md)`: Returns a substring that begins at the first matching occurrence of a specified character in a string.

    -   `[strjoin](function_strjoin.md)`: Concatenates two specified strings and returns the resulting string.

    -   `[strlen](function_strlen.md)`: Returns the length of a string in bytes.

    -   `[strrchr](function_strrchr.md)`: Returns a substring that begins at the last matching occurrence of a specified character in a string.

    -   `[strstr](function_strstr.md)`: Returns a substring starting at first occurrence of a specified substring within a string.

    -   `[strtok](function_strtok.md)`: Parse a string into a sequence of tokens using a specified delimiter.

    -   `[substr](function_substr.md)`: Returns the substring from a string at a specified index position.

-   **File Path Manipulation Functions**

    Similar to string manipulation functions, file path manipulation functions act on file paths or can provide the path name for a specified pointer. Some of these functions have analogs in the system library calls described in section 3 of the Linux manual pages.

    -   `[basename](function_basename.md)`: Returns a string excluding any prefix ending in `/`.

    -   `[dirname](function_dirname.md)`: Returns the path up to the last level of a specified string.

-   **Integer Conversion Functions**

    Similar to string manipulation functions, DTrace includes several integer conversion functions that can convert integers between host byte order and network byte order. These functions have analogs in the system library calls described in section 3 of the Linux manual pages.

    The following integer conversion functions are available:

    -   `[htonl](function_htonl.md)`: Converts an unsigned 32-bit long integer from host byte order to network byte order.

    -   `[htonll](function_htonll.md)`: Converts an unsigned 64-bit long integer from host byte order to network byte order.

    -   `[htons](function_htons.md)`: Converts a short 16-bit unsigned integer from host byte order to network byte order.

    -   `[ntohl](function_ntohl.md)`: Converts a 32-bit long integer from network byte order to host byte order.

    -   `[ntohll](function_ntohll.md)`: Converts a 64-bit long integer from network byte order to host byte order.

    -   `[ntohs](function_ntohs.md)`: Converts a short 16-bit integer from network byte order to host byte order.

-   **Copying Functions**

    Copying functions are functions that relate to copying information between memory addresses and DTrace buffers. Some of these functions are also considered process destructive functions because they change data in memory for a running process. Destructive functions must be explicitly enabled in DTrace.

    -   `[alloca](function_alloca.md)`: Allocates memory and returns a pointer.

    -   `[bcopy](function_bcopy.md)`: Copies a specified size in bytes from a specified source address outside of scratch memory to a destination address inside scratch memory.

    -   `[copyin](function_copyin.md)`: Copies the specified size from the user address to a DTrace buffer and returns the address of the buffer.

    -   `[copyinstr](function_copyinstr.md)`: Copies a null-terminated C string from the specified user address to a DTrace buffer and returns the address of the buffer.

    -   `[copyinto](function_copyinto.md)`: Copies the specified size in bytes from the specified user address into the DTrace scratch buffer and returns the buffer address.

    -   `[copyout](function_copyout.md)`: Copies the specified size from the specified DTrace buffer to the specified user space address.

    -   `[copyoutstr](function_copyoutstr.md)`: Copies a specified string to a specified user space address.

-   **Lock Analysis Functions**

    Lock analysis functions are used to check mutexes and file locks.

    The following lock analysis functions are available:

    -   `[mutex\_owned](function_mutex_owned.md)`: Checks whether a thread holds the specified kernel mutex.

    -   `[mutex\_owner](function_mutex_owner.md)`: Returns the thread pointer to the current owner of the specified kernel mutex.

    -   `[mutex\_type\_adaptive](function_mutex_type_adaptive.md)`: Returns a non zero value if a specified kernel mutex is adaptive.

    -   `[mutex\_type\_spin](function_mutex_type_spin.md)`: Returns a non zero value if a specified kernel mutex is a spin mutex.

    -   `[rw\_iswriter](function_rw_iswriter.md)`: Checks whether a writer is holding or waiting for the specified reader-writer lock.

    -   `[rw\_read\_held](function_rw_read_held.md)`: Checks whether the specified reader-writer lock is held by a reader.

    -   `[rw\_write\_held](function_rw_write_held.md)`: Checks whether the specified reader-writer lock is held by a writer.

-   **Symbolic Names and Stack Analysis Functions**

    DTrace includes functions that either record stack traces to the buffer or which can print symbols and module names for pointers to addresses in user space or kernel space can be helpful for debugging processes.

    The following functions return information about stack and addresses:

    -   `[stack](function_stack.md)`: Records a stack trace to the buffer.

    -   `[func](function_func.md)`: Prints the symbol for a specified kernel space address. An alias for `sym`.

    -   `[mod](function_mod.md)`: Prints the module name that corresponds to a specified kernel space address.

    -   `[sym](function_sym.md)`: Prints the symbol for a specified kernel space address. An alias for `func`.

    -   `[ustack](function_ustack.md)`: Records a user stack trace to the directed buffer.

    -   `[uaddr](function_uaddr.md)`: Prints the symbol for a specified address.

    -   `[ufunc](function_ufunc.md)`: Prints the symbol for a specified user space address. An alias for `usym`.

    -   `[umod](function_umod.md)`: Prints the module name that corresponds to a specified user space address.

    -   `[usym](function_usym.md)`: Prints the symbol for a specified address. An alias for `ufunc`.

-   **General System Functions**

    DTrace includes several functions to obtain information from the system or which are generalized for different use cases. Functions in this category include:

    -   `[getmajor](function_getmajor.md)`: Returns the major device number for a specified device.

    -   `[getminor](function_getminor.md)`: Returns the minor device number for a specified device

    -   `[inet\_ntoa](function_inet_ntoa.md)`: Returns a dotted, quad decimal string for a pointer to an IPv4 address.

    -   `[progenyof](function_progenyof.md)`: Checks whether a calling process is in the progeny of a specified process ID.

    -   `[rand](function_rand.md)`: Returns a pseudo random integer.

-   **Destructive Functions**

    DTrace is designed to run code safely. By using destructive functions, you must explicitly enable them to relax the constraints that protect a system from actions that are run from DTrace.

    Destructive functions can change a process or the entire system in some defined manner. These include functions such as stopping the current process, raising a specific signal on the current process or even spawning another system process. You can only use these functions if the facility to use destructive functions is explicitly enabled. When using the dtrace utility, you can enable destructive functions by using the `-w` command line option.

    If you try to use destructive functions without explicitly enabling them, `dtrace` fails with a message similar to the following:

    ```
    dtrace: failed to enable 'syscall': destructive functions not allowed
    ```

    These functions must be used with caution, as such functions can affect every process on the system and any other system, implicitly or explicitly, depending upon the affected system's network services.

    -   `[copyout](function_copyout.md)`: Copies the specified size from the specified DTrace buffer to the specified user space address.

    -   `[copyoutstr](function_copyoutstr.md)`: Copies a specified string to a specified user space address.

    -   `[freopen](function_freopen.md)`: Changes the file associated with `stdout` to a specified file.

    -   `[ftruncate](function_ftruncate.md)`: Truncates the output stream on `stdout`.

    -   `[raise](function_raise.md)`: Sends a specified signal to the running process.

    -   `[system](function_system.md)`: Causes a specified program to be run on the system as if within a shell.

-   **Special Functions**

    DTrace also includes functions that change DTrace behavior such as exiting tracing altogether or changing DTrace runtime options.

    -   `[exit](function_exit.md)`: Stops all tracing and exits to return an exit value.

    -   `[setopt](function_setopt.md)`: Dynamically sets DTrace compiler or runtime options.


-   **[Default Action](../reference/dtrace-ref-DefaultAction.md)**  
 The default action applies when DTrace encounters an empty clause for a probe. The default action is to trace the enabled probe identifier \(EPID\).
-   **[Unimplemented Functions](../reference/unimplemented_functions.md)**  

-   **[alloca](../reference/function_alloca.md)**  
 Allocates memory and returns a pointer.
-   **[avg](../reference/aggregation_avg.md)**  
 Stores the arithmetic average of the specified expressions in an aggregation.
-   **[basename](../reference/function_basename.md)**  
 Returns a string excluding any prefix ending in `/`.
-   **[bcopy](../reference/function_bcopy.md)**  
 Copies a specified size in bytes from a specified source address outside of scratch memory to a destination address inside scratch memory.
-   **[clear](../reference/function_clear.md)**  
 Clears the values from an aggregation while retaining aggregation keys.
-   **[cleanpath](../reference/dtrace-ref-cleanpath.md)**  
Creates a copy of a path without redundant elements.
-   **[commit](../reference/function_commit.md)**  
 Commits the speculative buffer, specified by ID, to the principal buffer.
-   **[copyin](../reference/function_copyin.md)**  
 Copies the specified size from the user address to a DTrace buffer and returns the address of the buffer.
-   **[copyinstr](../reference/function_copyinstr.md)**  
 Copies a null-terminated C string from the specified user address to a DTrace buffer and returns the address of the buffer.
-   **[copyinto](../reference/function_copyinto.md)**  
 Copies the specified size in bytes from the specified user address into the DTrace scratch buffer and returns the buffer address.
-   **[copyout](../reference/function_copyout.md)**  
 Copies the specified size from the specified DTrace buffer to the specified user space address.
-   **[copyoutstr](../reference/function_copyoutstr.md)**  
 Copies a specified string to a specified user space address.
-   **[count](../reference/aggregation_count.md)**  
 Stores an incremented count value in an aggregation.
-   **[denormalize](../reference/function_denormalize.md)**  
 Removes the normalization that's applied to a specified aggregation.
-   **[dirname](../reference/function_dirname.md)**  
 Returns the path up to the last level of a specified string.
-   **[discard](../reference/function_discard.md)**  
 Discards a speculative buffer specified by the provided speculation ID.
-   **[exit](../reference/function_exit.md)**  
 Stops all tracing and exits to return an exit value.
-   **[freopen](../reference/function_freopen.md)**  
 Changes the file associated with `stdout` to a specified file.
-   **[ftruncate](../reference/function_ftruncate.md)**  
 Truncates the output stream on `stdout`.
-   **[func](../reference/function_func.md)**  
 Prints the symbol for a specified kernel space address. An alias for `sym`.
-   **[getmajor](../reference/function_getmajor.md)**  
 Returns the major device number for a specified device.
-   **[getminor](../reference/function_getminor.md)**  
 Returns the minor device number for a specified device
-   **[htonl](../reference/function_htonl.md)**  
 Converts an unsigned 32-bit long integer from host byte order to network byte order.
-   **[htonll](../reference/function_htonll.md)**  
 Converts an unsigned 64-bit long integer from host byte order to network byte order.
-   **[htons](../reference/function_htons.md)**  
 Converts a short 16-bit unsigned integer from host byte order to network byte order.
-   **[index](../reference/function_index.md)**  
 Finds the first occurrence of a substring within a string.
-   **[inet\_ntoa](../reference/function_inet_ntoa.md)**  
 Returns a dotted, quad decimal string for a pointer to an IPv4 address.
-   **[inet\_ntoa6](../reference/function_inet_ntoa6.md)**  
 Returns an RFC 1884 convention 2 string for a pointer to an IPv6 address.
-   **[inet\_ntop](../reference/function_inet_ntop.md)**  
 Returns an RFC 1884 convention 2 string for a pointer to an IPv6 address.
-   **[link\_ntop](../reference/dtrace-ref-link-ntop.md)**  
Returns a string translation of a hardware address.
-   **[llquantize](../reference/aggregation_llquantize.md)**  
 Stores the log-linear frequency distribution in an aggregation.
-   **[lltostr](../reference/function_lltostr.md)**  
 Converts an unsigned 64-bit integer to a string.
-   **[lquantize](../reference/aggregation_lquantize.md)**  
 Stores the linear frequency distribution of the values of the specified expressions, sized by the specified range, in an aggregation.
-   **[max](../reference/aggregation_max.md)**  
 Stores the largest value among the specified expressions in an aggregation.
-   **[min](../reference/aggregation_min.md)**  
 Stores the smallest value among the specified expressions in an aggregation.
-   **[mod](../reference/function_mod.md)**  
 Prints the module name that corresponds to a specified kernel space address.
-   **[mutex\_owned](../reference/function_mutex_owned.md)**  
 Checks whether a thread holds the specified kernel mutex.
-   **[mutex\_owner](../reference/function_mutex_owner.md)**  
 Returns the thread pointer to the current owner of the specified kernel mutex.
-   **[mutex\_type\_adaptive](../reference/function_mutex_type_adaptive.md)**  
 Returns a non zero value if a specified kernel mutex is adaptive.
-   **[mutex\_type\_spin](../reference/function_mutex_type_spin.md)**  
 Returns a non zero value if a specified kernel mutex is a spin mutex.
-   **[normalize](../reference/function_normalize.md)**  
 Divides an aggregation value by a specified normalization factor.
-   **[ntohl](../reference/function_ntohl.md)**  
 Converts a 32-bit long integer from network byte order to host byte order.
-   **[ntohll](../reference/function_ntohll.md)**  
 Converts a 64-bit long integer from network byte order to host byte order.
-   **[ntohs](../reference/function_ntohs.md)**  
 Converts a short 16-bit integer from network byte order to host byte order.
-   **[print](../reference/function_print.md)**  
 Prints information about a variable.
-   **[printa](../reference/function_printa.md)**  
 Displays and controls the formatting of an aggregation
-   **[printf](../reference/function_printf.md)**  
 Displays and controls the formatting of a string.
-   **[progenyof](../reference/function_progenyof.md)**  
 Checks whether a calling process is in the progeny of a specified process ID.
-   **[quantize](../reference/aggregation_quantize.md)**  
 Stores a power-of-two frequency distribution of the values of the specified expressions in an aggregation. An optional increment can be specified.
-   **[raise](../reference/function_raise.md)**  
 Sends a specified signal to the running process.
-   **[rand](../reference/function_rand.md)**  
 Returns a pseudo random integer.
-   **[rindex](../reference/function_rindex.md)**  
 Finds the last occurrence of a specific substring within a string.
-   **[rw\_iswriter](../reference/function_rw_iswriter.md)**  
 Checks whether a writer is holding or waiting for the specified reader-writer lock.
-   **[rw\_read\_held](../reference/function_rw_read_held.md)**  
 Checks whether the specified reader-writer lock is held by a reader.
-   **[rw\_write\_held](../reference/function_rw_write_held.md)**  
 Checks whether the specified reader-writer lock is held by a writer.
-   **[setopt](../reference/function_setopt.md)**  
 Dynamically sets DTrace compiler or runtime options.
-   **[speculate](../reference/function_speculate.md)**  
 A special function that causes DTrace to switch to using a speculation buffer identified by the specified ID for the remainder of a clause.
-   **[speculation](../reference/function_speculation.md)**  
 Creates a speculative trace buffer and returns its ID.
-   **[stack](../reference/function_stack.md)**  
 Records a stack trace to the buffer.
-   **[stddev](../reference/aggregation_stddev.md)**  
 Stores the standard deviation of the specified expressions in an aggregation.
-   **[strchr](../reference/function_strchr.md)**  
 Returns a substring that begins at the first matching occurrence of a specified character in a string.
-   **[strjoin](../reference/function_strjoin.md)**  
 Concatenates two specified strings and returns the resulting string.
-   **[strlen](../reference/function_strlen.md)**  
 Returns the length of a string in bytes.
-   **[strrchr](../reference/function_strrchr.md)**  
 Returns a substring that begins at the last matching occurrence of a specified character in a string.
-   **[strstr](../reference/function_strstr.md)**  
 Returns a substring starting at first occurrence of a specified substring within a string.
-   **[strtok](../reference/function_strtok.md)**  
 Parse a string into a sequence of tokens using a specified delimiter.
-   **[substr](../reference/function_substr.md)**  
 Returns the substring from a string at a specified index position.
-   **[sum](../reference/aggregation_sum.md)**  
 Stores the total value of the specified expression in an aggregation.
-   **[sym](../reference/function_sym.md)**  
 Prints the symbol for a specified kernel space address. An alias for `func`.
-   **[system](../reference/function_system.md)**  
 Causes a specified program to be run on the system as if within a shell.
-   **[trace](../reference/function_trace.md)**  
 Traces the result of an expression to the directed buffer.
-   **[tracemem](../reference/function_tracemem.md)**  
 Copies the specified number of bytes of data from an address in memory to the current buffer.
-   **[uaddr](../reference/function_uaddr.md)**  
 Prints the symbol for a specified address.
-   **[ufunc](../reference/function_ufunc.md)**  
 Prints the symbol for a specified user space address. An alias for `usym`.
-   **[umod](../reference/function_umod.md)**  
 Prints the module name that corresponds to a specified user space address.
-   **[ustack](../reference/function_ustack.md)**  
 Records a user stack trace to the directed buffer.
-   **[usym](../reference/function_usym.md)**  
 Prints the symbol for a specified address. An alias for `ufunc`.

