
# Use Predicates For Control Flow <a id="dt_preds_dlang">

For runtime safety, one major difference between D and other programming languages such as C, C++, and the Java programming language is the absence of control-flow constructs such as `if`-statements and loops. D program clauses are written as single straight-line statement lists that trace an optional, fixed amount of data. D does provide the ability to conditionally trace data and change control flow using logical expressions called *predicates*. This tutorial shows how to use predicates to control D programs.

To illustrate predicates at work, you can create a D program that implements a 10-second countdown timer. When the program runs, it counts down from 10 and then prints a message and exits. The program uses a variable and predicates to evaluate how much time has passed and what to print.

1.  Design a logical flow for the program.

    Consider designing the logical flow for a program before trying to write the program itself. When the flow is clearly defined, it's possible to transform conditional constructs into separate clauses and predicates. The logical flow for the program might look as follows:

    ```
    i = 10
    once per second,
      if i is greater than zero
        trace(i--);
      if i is equal to zero
        trace("blastoff!");
        exit(0);
    ```

    By creating two clauses with the same probe description but different predicates and functions it's possible to achieve the required logical flow for this program.

2.  Write the program code using predicates to decide whether the functions for the specified probe description are permitted to run or not when the probe fires.

    The program source code follows. Copy this code and save it in a file named `countdown.d`:

    ```
    dtrace:::BEGIN 
    {
      i = 10;
    }
    
    profile:::tick-1sec
    /i > 0/
    {
      trace(i--);
    }
    
    profile:::tick-1sec
    /i == 0/
    {
      trace("blastoff!");
      exit(0);
    }
    ```

3.  Run the program.

    ```
    sudo dtrace -s countdown.d
    ```

    Output similar to the following is displayed:

    ```
    dtrace: script 'countdown.d' matched 3 probes
    CPU     ID                    FUNCTION:NAME
      0    638                       :tick-1sec        10
      0    638                       :tick-1sec         9
      0    638                       :tick-1sec         8
      0    638                       :tick-1sec         7
      0    638                       :tick-1sec         6
      0    638                       :tick-1sec         5
      0    638                       :tick-1sec         4
      0    638                       :tick-1sec         3
      0    638                       :tick-1sec         2
      0    638                       :tick-1sec         1
      0    638                       :tick-1sec   blastoff!       
    #
    ```


This tutorial uses the `BEGIN` probe to initialize a variable integer `i` to 10 to begin the countdown. Next, the program uses the `tick-1sec` probe to implement a timer that fires once every second. Notice that in `countdown.d`, the `tick-1sec` probe description is used in two different clauses, each with a different predicate and function list. The predicate is a logical expression surrounded by enclosing slashes `//` that appears after the probe name and before the braces `{}` that surround the clause statement list.

The first predicate tests whether `i` is greater than zero, indicating that the timer is still running:

```
profile:::tick-1sec
/i > 0/
{
  trace(i--);
}
```

The relational operator `>` means *greater than* and returns the integer value zero for false and one for true. If `i` isn't yet zero, the script traces `i` and then decrements it by one using the `--` operator.

The second predicate uses the `==` operator to return true when `i` is exactly equal to zero, indicating that the countdown is complete:

```
profile:::tick-1sec
/i == 0/
{
  trace("blastoff!");
  exit(0);
}
```

The second clause uses the `trace` function on a sequence of characters inside double quotes, called a *string constant*, to print a final message when the countdown is complete. The `exit` function is then used to end all tracing and to perform any remaining tasks such as consuming the final data, printing aggregations \(as needed\), and performing cleanup before returning to the shell prompt.

## How to use a predicate to monitor system calls for a process ID

You can create a D Program to trace system calls for a process ID, by using a predicate to limit the default tracing function to match the process ID that you want to trace.

```
syscall:::entry
/pid == 2860/
{
}
```

Note that in this example, the built-in variable `pid` is evaluated to match a particular ID, `2860` in this example. You could further change this script to take advantage of shell macro variables, so that it becomes more extensible and can be run for any process ID at runtime. Edit the script as follows and save it to a file called `strace.ds`:

```
#!/usr/sbin/dtrace -s

syscall:::entry
/pid == $1/
{
}
```

Change the file mode to make it executable:

```
sudo chmod +x strace.ds
```

Now you can use this script to monitor all the system calls made by any process on the system. For example, you could run the script to monitor system calls made by the cron daemon:

```
sudo ./strace.ds $(pidof /usr/sbin/crond)
```

**Parent topic:**[Get Started With DTrace](../how-to/dtrace-guide.md)

