
# denormalize

Removes the normalization that's applied to a specified aggregation.

```
void denormalize(@ *aggr*)
```

The `denormalize` function removes any normalization that's applied to a specified aggregation. Normalization doesn't change the underlying data that makes up an aggregation, so the `denormalize` function removes the normalization to return the raw data directly.

## How denormalize is used in a script to present raw data

```
#pragma D option quiet

BEGIN
{
  start = timestamp;
}

syscall:::entry
{
  @func[execname] = count();
}

END
{
  this->seconds = (timestamp - start) / 1000000000;
  printf("Ran for %d seconds.\n", this->seconds);
  printf("Per-second rate:\n");
  normalize(@func, this->seconds);
  printa(@func);
  printf("\nRaw counts:\n");
  denormalize(@func);
  printa(@func);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

