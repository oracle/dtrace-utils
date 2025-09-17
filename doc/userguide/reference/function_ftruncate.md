
# ftruncate

Truncates the output stream on `stdout`.

```
void ftruncate()
```

The `ftruncate` function is a data recording function that truncates the output stream on `stdout`.

## How to use ftruncate to truncate the stdout output stream, by using a counter

```
tick-10ms
{
    printf("%d\n", i++);
}

tick-10ms
/i == 10/
{
    ftruncate();
}

tick-10ms
/i == 20/
{
    exit(0);
}
```

When the example script is run using `sudo dtrace -o /tmp/result -s */path/to/script*`. Standard output is saved to `/tmp/result`. The program implements a counter that's triggered every 10 ms and is designed to count up to 20 before exiting. The counter prints to standard output for every count, but when the counter reaches 10, `ftruncate` is called to truncate standard output. When the program exits and you can view the contents of `/tmp/result` you can see that the standard output preceding the 11th counter is removed.

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

