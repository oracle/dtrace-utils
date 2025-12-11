
# Aggregations <a id="dt_ref_aggregations">

Aggregations enable you to accumulate data for statistical analysis. The aggregation is calculated at runtime, so that post-processing isn't required and processing is highly efficient and accurate. Aggregations function similarly to associative arrays, but are populated by aggregating functions. In D, the syntax for an aggregation is as follows:

```
@*name*[ *keys* ] = *aggfunc*( *args* );
```

The aggregation *name* is a D identifier that's prefixed with the special character `@`. All aggregations that are named in D programs are global variables. Aggregations can't have thread-local or clause-local scope. The aggregation names are kept in an identifier namespace that's separate from other D global variables. If you reuse names, remember that `a` and `@a` are *not* the same variable. The special aggregation name `@` can be used to name an anonymous aggregation in D programs. The D compiler treats this name as an alias for the aggregation name `@_`.

Aggregations can be regular or indexed. Indexed aggregations use keys, where *keys* are a comma-separated list of D expressions, similar to the tuples of expressions used for associative arrays. Regular aggregations are treated similarly to indexed aggregations, but don't use keys for indexing.

The *aggfunc* is one of the DTrace aggregating functions, and *args* is a comma-separated list of arguments appropriate to that function. Most aggregating functions take a single argument that represents the new datum.

## Aggregation Functions <a id="dt_ref_aggr_funcs">

The following functions are aggregating functions that can be used in a program to collect data and present it in a meaningful way.

-   [`avg`](aggregation_avg.md): Stores the arithmetic average of the specified expressions in an aggregation.

-   [`count`](aggregation_count.md): Stores an incremented count value in an aggregation.

-   [`max`](aggregation_max.md): Stores the largest value among the specified expressions in an aggregation.

-   [`min`](aggregation_min.md): Stores the smallest value among the specified expressions in an aggregation.

-   [`sum`](aggregation_sum.md): Stores the total value of the specified expression in an aggregation.

-   [`stddev`](aggregation_stddev.md): Stores the standard deviation of the specified expressions in an aggregation.

-   [`quantize`](aggregation_quantize.md): Stores a power-of-two frequency distribution of the values of the specified expressions in an aggregation.
    An optional increment can be specified.

-   [`lquantize`](aggregation_lquantize.md): Stores the linear frequency distribution of the values of the specified expressions,
    sized by the specified range, in an aggregation.

-   [`llquantize`](aggregation_llquantize.md): Stores the log-linear frequency distribution in an aggregation.


## Printing Aggregations <a id="dt_ref_aggr_print">

By default, several aggregations are displayed in the order in which they're introduced in the D program.
You can override this behavior by using the [`printa`](function_printa.md) function to print the aggregations.
The `printa` function also lets you precisely format the aggregation data by using a format string.

If an aggregation isn't formatted with a `printa` statement in a D program, the `dtrace` command snapshots the aggregation data and prints the results after tracing has completed, using the default aggregation format. If an aggregation is formatted with a `printa` statement, the default behavior is disabled. You can achieve the same results by adding the `printa(@*aggregation-name*)` statement to an `END` probe clause in a program.

The default output format for the `avg`, `count`, `min`, `max`, `stddev`, and `sum` aggregating functions displays an integer decimal value corresponding to the aggregated value for each tuple. The default output format for the `quantize`, `lquantize`, and `llquantize` aggregating functions displays an ASCII histogram with the results. Aggregation tuples are printed as though `trace` had been applied to each tuple element.

## Data Normalization <a id="dt_ref_aggr_dnorm">

When aggregating data over some period, you might want to normalize the data based on some constant factor.
This technique lets you compare disjointed data more easily.
For example, when aggregating system calls,
you might want to output system calls as a per-second rate instead of as an absolute value over the course of the run.
The DTrace [`normalize`](function_normalize.md) function lets you normalize data in this way.
The parameters to `normalize` are an aggregation and a normalization factor.
The output of the aggregation shows each value divided by the normalization factor.

**Parent topic:**[D Program Syntax Reference](../reference/d_program_syntax_reference.md)
