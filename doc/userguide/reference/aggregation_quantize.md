
# quantize

Stores a power-of-two frequency distribution of the values of the specified expressions in an aggregation. An optional increment can be specified.

```
void quantize(*expr* [, uint32_t *incr*])
```

The `quantize` function is an aggregation function to distribution of information in a histogram for an expression, *expr*. An optional integer value, *incr*, can be specified to find the amount that the values are incremented by to weight the output. This function makes it easier to see a graphical representation of the values returned by an expression.

The rows for the frequency distribution are always power-of-two values. Each row indicates a count of the number of elements that are greater than or equal to the corresponding value, but less than the next larger row's value.

## How to use quantize to display the distribution of write\(\) call times by process

```
syscall::write:entry
{
  self->ts = timestamp;
}

syscall::write:return
/self->ts/
{
  @time[execname] = quantize(timestamp - self->ts);
  self->ts = 0;
}
```

Output similar to the following is displayed after the program exits:

```
  bash                                              
           value  ------------- Distribution ------------- count    
            8192 |                                         0        
           16384 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@         4        
           32768 |                                         0        
           65536 |                                         0        
          131072 |@@@@@@@@                                 1        
          262144 |                                         0        

  gnome-terminal                                    
           value  ------------- Distribution ------------- count    
            4096 |                                         0        
            8192 |@@@@@@@@@@@@@                            5        
           16384 |@@@@@@@@@@@@@                            5        
           32768 |@@@@@@@@@@@                              4        
           65536 |@@@                                      1        
          131072 |                                         0        

  Xorg                                              
           value  ------------- Distribution ------------- count    
            2048 |                                         0        
            4096 |@@@@@@@                                  4        
            8192 |@@@@@@@@@@@@@                            8        
           16384 |@@@@@@@@@@@@                             7        
           32768 |@@@                                      2        
           65536 |@@                                       1        
          131072 |                                         0        
          262144 |                                         0        
          524288 |                                         0        
         1048576 |                                         0        
         2097152 |@@@                                      2        
         4194304 |                                         0        

  firefox                                           
           value  ------------- Distribution ------------- count    
            2048 |                                         0        
            4096 |@@@                                      22       
            8192 |@@@@@@@@@@@                              90       
           16384 |@@@@@@@@@@@@@                            107      
           32768 |@@@@@@@@@                                72       
           65536 |@@@                                      28       
          131072 |                                         3        
          262144 |                                         0        
          524288 |                                         1        
         1048576 |                                         1        
         2097152 |                                         0
...
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

