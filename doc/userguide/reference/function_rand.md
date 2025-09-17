
# rand

Returns a pseudo random integer.

```
int rand(void)
```

The `rand` function returns a pseudo random integer. The value returned is a weak pseudo random number and we don't recommend using it for any cryptographic application.

## How to use rand to generate a pseudo random integer

The example uses the trace function to print the generated integer in the trace output.

```
BEGIN{ 
   trace(rand()); 
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

