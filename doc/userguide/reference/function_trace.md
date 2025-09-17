
# trace

Traces the result of an expression to the directed buffer.

```
void trace(*expr*)
```

The `trace` function is the most fundamental DTrace function. This function takes a D expression as its argument and then traces the result to the directed buffer.

If the `trace` function is used on a buffer, the output format depends on the data type. If the data is 1, 2, 4, or 8 bytes in size, the result is formatted as a decimal integer value. If the data is any other size, and is a sequence of printable characters if interpreted as a sequence of bytes, it's printed as an ASCII string and ends with a null character \(`0`\). If the data is any other size, and isn't a sequence of printable characters, it's printed as a series of byte values that's formatted as hexadecimal integers.

You can force the `trace` function to always use the binary format by specifying the `rawbytes` dynamic runtime option.

## How to use trace to display a variety of different outputs

The example shows the trace function being used to return output for a built-in variable, an expression, and a string value.

```
BEGIN
{
trace(execname);
trace(timestamp / 1000);
trace("somehow managed to get here");
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

