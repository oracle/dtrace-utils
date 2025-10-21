
# Create a DTrace Script <a id="dt_create_script">

Learn how to create a DTrace script to develop understanding of the D Programming language.

Ensure that DTrace is installed on the system and that you can list and enable probes. See [Install DTrace](../how-to/dtrace-howto-install-dtrace.md) and [List and Enable Probes](../how-to/dtrace-howto-list-and-enable-probes.md).

This tutorial provides successive steps toward developing a DTrace script that you can use on a system to gather useful information. You can use this tutorial as a framework to create other scripts for DTrace, in future.

1.  In a text editor, create a file named `hello.d` and write a DTrace clause to fire for the `dtrace:::BEGIN` probe.

    Enter the following text into the editor:

    ```
    dtrace:::BEGIN
    {
      trace("hello, world");
      exit(0);
    }
    ```

    Save the file.

2.  Run the `hello.d` program by using the `dtrace -s` command.

    ```
    sudo dtrace -s hello.d
    ```

    Output similar to the following is displayed:

    ```
    dtrace: script 'hello.d' matched 1 probe
    CPU     ID                    FUNCTION:NAME
      0      1                           :BEGIN   hello, world    
    ```

    Note that you didn't have to press `Ctrl`+`C` to exit because you specified the `exit` function for the `BEGIN` probe in the program.

3.  Open `hello.d` in the text editor and add an interpreter line to the beginning of the script.

    Edit the file and add the following line of text to the top of the file:

    ```
    #!/usr/sbin/dtrace -s
    ```

    The complete script follows:

    ```
    #!/usr/sbin/dtrace -s
    dtrace:::BEGIN
    {
      trace("hello, world");
      exit(0);
    }
    ```

    Save the file.

4.  Change the permissions on the `hello.d` file to make it executable.

    Run the `chmod` command to update the file permissions:

    ```
    chmod a+rx hello.d
    ```

5.  Run the new executable script file.

    Use the `sudo` command so that the DTrace script still runs with root privileges so that it can access all DTrace features:

    ```
    sudo ./hello.d
    ```

    Note that by including an interpreter line at the beginning of the program, you can run the script without even specifying the `dtrace` command.

6.  Change the script to use an external macro variable.

    Edit the file to greet a person by name, when you specify a name as an argument to the script:

    ```
    #!/usr/sbin/dtrace -s
    dtrace:::BEGIN
    {
      printf("hello, %s", $$1);
      exit(0);
    }
    ```

    Notice how the `trace` function is now replaced with the `printf()` function, which lets you insert the macro variable `$1` into the string by using variable substitution. The `$$` syntax is used when referencing the macro variable, to express it as a string value.

7.  Run the script to see how the modification has altered behavior.

    Run the script as before, using the command:

    ```
    sudo ./hello.d
    ```

    An error similar to the following is generated.

    ```
    dtrace: failed to compile script ./hello.d: line 4: macro argument $$1 is not defined
    ```

    The error is generated because the script now expects you to provide another argument when you run it. Try to run the script again, this time specifying a name:

    ```
    sudo ./hello.d *bob*
    ```

    The script returns output similar to the following:

    ```
    dtrace: script './hello.d' matched 1 probe
    CPU     ID                    FUNCTION:NAME
      3      1                           :BEGIN hello, bob
    ```

8.  Change the script to use a pragma statement.

    To reduce how verbose the script is and to limit output to only what's functionally returned by the clause, add a pragma statement to set the runtime `quiet` option. Edit the script to add the pragma statement, as follows:

    ```
    #!/usr/sbin/dtrace -s
     #pragma D option quiet
     dtrace:::BEGIN
     {
       printf("hello, %s", $$1);
       exit(0);
     }
    ```

9.  Run the script to see how the modification has altered behavior.

    Run the script as before, using the command:

    ```
    sudo ./hello.d sally
    ```

    The script output is reduced to only what's returned by the `printf()` function.

10. Change the script to use a predicate to control when to process the clause.

    You can use a predicate to control the script so that it only runs when a certain condition is true. Edit the script to add a predicate line to evaluate whether the string value of the macro variable is equal to '*bob*', as follows:

    ```
    #!/usr/sbin/dtrace -s
     #pragma D option quiet
    
     dtrace:::BEGIN
     /$$1=="bob"/
     {
       printf("hello, %s", $$1);
       exit(0);
     }
    ```

11. Run the script to see how the modification has altered behavior.

    Run the script as before, using the command:

    ```
    sudo ./hello.d sally
    ```

    The script doesn't exit and you need to press `Ctrl`+`C` to force quit the process. This is because the `exit()` function is part of the clause that evaluates whether the first argument of the script is equal to '*bob*'. Try running the script again, using *bob* as the argument.

    ```
    sudo ./hello.d bob
    ```

    The script runs as before, illustrating that the predicate is working.


**Parent topic:**[Get Started With DTrace](../how-to/dtrace-guide.md)

