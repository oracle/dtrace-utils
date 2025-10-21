
# List and Enable Probes <a id="list_enable_probes">

DTrace providers publish available probes to DTrace so that you can enable them to perform functions when they fire. You can use the `dtrace` command to list all available probes or to enable a probe.

1.  List available probes.

    To list all available probes, run:

    ```
    sudo dtrace -l
    ```

    **Note:**

    Most uses of DTrace require `root` privileges. This document assumes that you run commands with the appropriate privileges. Use the sudo command to escalate to root user privileges before you run the commands presented in this document.

    The command returns output similar to the following:

    ```
    DTrace 2.0.0 [Pre-Release with limited functionality]
        ID   PROVIDER            MODULE                          FUNCTION NAME
         1     dtrace                                                     BEGIN
         2     dtrace                                                     END
         3     dtrace                                                     ERROR
         4        fbt           vmlinux        __traceiter_initcall_level entry
         5        fbt           vmlinux        __traceiter_initcall_level return
         6        fbt           vmlinux        __traceiter_initcall_start entry
         7        fbt           vmlinux        __traceiter_initcall_start return
         8        fbt           vmlinux       __traceiter_initcall_finish entry
         9        fbt           vmlinux       __traceiter_initcall_finish return
    ...
    144917        sdt               rtc                                   rtc_set_time
    144918        sdt               i2c                                   i2c_result
    144919        sdt               i2c                                   i2c_reply
    144920        sdt               i2c                                   i2c_read
    144921        sdt               i2c                                   i2c_write
    144922        sdt             smbus                                   smbus_result
    144923        sdt             smbus                                   smbus_reply
    144924        sdt             smbus                                   smbus_read
    144925        sdt             smbus                                   smbus_write
    144926        sdt             hwmon                                   hwmon_attr_show_string
    144927        sdt             hwmon                                   hwmon_attr_store
    144928        sdt             hwmon                                   hwmon_attr_show
    144929        sdt           thermal                                   thermal_zone_trip
    144930        sdt           thermal                                   cdev_update
    144931        sdt           thermal                                   thermal_temperature
    144932        sdt            bcache    
    ...
    145763    syscall           vmlinux                            listen entry
    145764    syscall           vmlinux                              bind return
    145765    syscall           vmlinux                              bind entry
    145766    syscall           vmlinux                        socketpair return
    145767    syscall           vmlinux                        socketpair entry
    145768    syscall           vmlinux                            socket return
    145769    syscall           vmlinux                            socket entry
    ```

    **Tip:** You can get a unique list of providers available for DTrace by running:

    ```
    sudo dtrace -l|tail -n +3|awk '{print $2}'|uniq
    ```

    You can limit the list of probes to a particular provider by using the `-P` option. You can also limit to a particular module by using the `-m` option. For example:

    ```
    sudo dtrace -l -P sdt
    sudo dtrace -l -m thermal
    ```

2.  Run `dtrace -n` to enable a named probe using the command line utility.

    You can enable any probe matching a name. Although you can specify only the name part for a probe's full name, using the full name helps to avoid unpredictable behavior:

    ```
    sudo dtrace -n dtrace:::BEGIN
    ```

    Output similar to the following is displayed:

    ```
    dtrace: description 'dtrace:::BEGIN' matched 1 probe
    CPU     ID                    FUNCTION:NAME
      2      1                           :BEGIN
    ```

    The `dtrace:::BEGIN` probe fires once when you start a new tracing request. Tabulated output shows the CPU where the probe fired, and the ID, function, and name for the probe.

    DTrace continues to run, waiting for other probes to fire. To exit, press `Ctrl`+`C`.

3.  Enable several probes by chaining them together in a request.

    You can construct DTrace requests by using arbitrary numbers of probes and functions. For example, create a request using two probes by adding the `BEGIN` and `END` probes.

    Type the following command, and then press `Ctrl`+`C` in the shell again, after you see the line of output for the `BEGIN` probe:

    ```
    sudo dtrace -n dtrace:::BEGIN -n dtrace:::END 
    ```

    The output looks similar to:

    ```
    dtrace: description 'dtrace:::BEGIN' matched 1 probe
    dtrace: description 'dtrace:::END' matched 1 probe
    CPU     ID                    FUNCTION:NAME
      0      1                           :BEGIN 
    `^C`
      1      2                             :END
    ```

    The `dtrace:::BEGIN` probe fires when the tracing request starts. DTrace waits for further probes to activate until you press `Ctrl`+`C` to exit. The `dtrace:::END` probe activates once when tracing completes. The `dtrace` command reports the probe firing before exiting.

4.  Enable all probes for a function by using the -f option, or use the -m option to enable all probes for a module.

    You can match and enable probes for functions or for whole modules. For example, to enable both the entry and return probes for the `syscall:vmlinux:socket` function, run:

    ```
    sudo dtrace -f syscall:vmlinux:socket
    ```

    You can also enable probes for an entire module. For example, to enable all probes for the `sdt:tcp` module, run:

    ```
    sudo dtrace -m sdt:tcp
    ```


**Parent topic:**[Get Started With DTrace](../how-to/dtrace-guide.md)

