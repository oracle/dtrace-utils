
# llquantize

Stores the log-linear frequency distribution in an aggregation.

```
void llquantize(*expr*, int32_t *factor*, int32_t *from*, int32_t *to* [, int32_t *steps* [, int32_t *incr*]])
```

The `llquantize` function is an aggregation function used to display a log-linear frequency distribution for an expression. The logarithmic base, *factor*, is specified along with lower, *from*, and upper, *to*, exponents and the number of steps, *steps*, per order of magnitude. If the number of steps isn't provided, a default value of 1 is used. An optional integer, *incr*, can be provided to specify the amount to increment each step by.

The log-linear `llquantize` aggregating function combines the capabilities of both the log and linear functions. While the `quantize` function uses base 2 logarithms, with `llquantize`, you specify the base, and the minimum and, maximum exponents. Further, each logarithmic range is subdivided linearly by the number of steps specified and the increment value, if specified.

## How to use llquantize to visualize system call latencies

The script monitors all system call entry and return calls. The time spent in each call is calculated using the timestamp for each. An aggregation is used to create a log-linear quantization with factor of 10 ranging from magnitude 3 to magnitude 5 \(inclusive\) with 10 steps per magnitude. The output from this script visualizes the latency of system calls in the microsecond range.

```
syscall:::entry
{
  self->ts = timestamp;
}

syscall:::return
/ self->ts /
{
  @ = llquantize(timestamp - self->ts, 10, 3, 5, 5);
  self->ts = 0;
}
```

Output similar to the following is displayed:

```
           value  ------------- Distribution ------------- count    
           -1000 |                                         0        
    abs() < 1000 |@@@@@@@@@@@@@@@                          2888133  
            1000 |@@@@@                                    1017345  
            2000 |@@@@                                     714432   
            4000 |@                                        266057   
            6000 |@                                        118797   
            8000 |                                         84332    
           10000 |@                                        152108   
           20000 |@                                        125154   
           40000 |                                         49334    
           60000 |                                         38374    
           80000 |                                         31739    
          100000 |                                         91033    
          200000 |                                         51153    
          400000 |                                         20343    
          600000 |                                         10685    
          800000 |                                         6970     
      >= 1000000 |@@@@@@@@@@@                              2081856  
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

