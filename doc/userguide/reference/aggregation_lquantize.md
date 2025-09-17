
# lquantize

Stores the linear frequency distribution of the values of the specified expressions, sized by the specified range, in an aggregation.

```
void lquantize(*expr*, int32_t *from*, int32_t *to* [, int32_t *step*])
```

The `lquantize` function is an aggregation function used to display a linear value distribution. The `lquantize` function takes four arguments: a D expression, *expr*, a lower bound, *from*, an upper bound, *to*, and an optional *step*. Note that the default step value is `1`.

## How to use lquantize to display the distribution of write\(\) calls by file descriptor

```
syscall::write:entry
{
  @fds[execname] = lquantize(arg0, 0, 100, 1);
}
```

Output similar to the following might be displayed after the program exits:

```
 ...
  gnome-session                                     
           value  ------------- Distribution ------------- count    
              25 |                                         0        
              26 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 9        
              27 |                                         0        

  gnome-terminal                                    
           value  ------------- Distribution ------------- count    
              15 |                                         0        
              16 |@@                                       1        
              17 |                                         0        
              18 |                                         0        
              19 |                                         0        
              20 |                                         0        
              21 |@@@@@@@@                                 4        
              22 |@@                                       1        
              23 |@@                                       1        
              24 |                                         0        
              25 |                                         0        
              26 |                                         0        
              27 |                                         0        
              28 |                                         0        
              29 |@@@@@@@@@@@@@                            6        
              30 |@@@@@@@@@@@@@                            6        
              31 |                                         0        
 ...
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

