
# Variables <a id="dt_vars_dlang">

D provides several variable types: scalar variables, associative arrays, scalar arrays, and multidimensional scalar arrays. Variables can be created by declaring them explicitly, but are most often created implicitly on first use. Variables can be restricted to clause or thread scope to avoid name conflicts and to control the lifetime of a variable explicitly.

-   **Scalar Variables**

    Scalar variables are used to represent individual, fixed-size data objects, such as integers and pointers. Scalar variables can also be used for fixed-size objects that are composed of one or more primitive or composite types. D provides the ability to create arrays of objects and composite structures. DTrace also represents strings as fixed-size scalars by permitting them to grow to a predefined maximum length.

    To create a scalar variable, you can write an assignment expression of the following form:

    ```
    *name* = *expression* ;
    ```

    where *name* is any valid D identifier and *expression* is any value or expression that the variable contains.

    DTrace includes several built-in scalar variables that can be referenced within D programs. The values of these variables are automatically populated by DTrace. See [DTrace Built-in Variable Reference](dtrace_builtin_variable_reference.md) for a complete list of these variables.

-   **Associative Arrays**

    Associative arrays are used to represent collections of data elements that can be retrieved by specifying a *key*. Associative arrays differ from normal, fixed-size arrays in that they have no predefined limit on the number of elements and can use any expression as a key. Furthermore, elements in an associative array aren't stored in consecutive storage locations.

    To create an associative array, you can write an assignment expression of the following form:

    ```
    *name* [ *key* ] = *expression* ;
    ```

    Where *name* is any valid D identifier, *key* is a comma-separated list of one or more expressions, often as string values, and *expression* is the value that's contained by the array for the specified key.

    The type of each object that's contained in the array is also fixed for all elements in the array. You can use any of the assignment operators that are defined in [Types, Operators, and Expressions](dtrace-ref-TypesOperatorsandExpressions.md) to change associative array elements, subject to the operand rules defined for each operator. The D compiler produces an appropriate error message if you try an incompatible assignment. You can use any type with an associative array key or value that can be used with a scalar variable.

    You can reference values in an associative array by specifying the array name and the appropriate key.

    You can delete an element in an associative array by assigning a literal `0` to it, regardless of the element datatype. When you delete elements in an array, the storage that's used for that element is deallocated and made available to the system for use.

-   **Scalar Arrays**

    Scalar arrays are a fixed-length group of consecutive memory locations that each store a value of the same type. Scalar arrays are accessed by referring to each location with an integer, starting from zero. Scalar arrays aren't used as often in D as associative arrays.

    A D scalar array of 5 integers is declared by using the type `int` and suffixing the declaration with the number of elements in square brackets, for example:

    ```
    int s[5];
    ```

    The D expression `s[0]` refers to the first array element, `s[1]` refers to the second, and so on. DTrace performs bounds checking on the indexes of scalar arrays at compile time to help catch bad index references early.

    **Note:**

    Scalar arrays and associative arrays are syntactically similar. You can declare an associative array of integers referenced by an integer key as follows:

    ```
    int a[int];
    ```

    You can also reference this array using the expression `a[0]`, but from a storage and implementation perspective, the two arrays are different. The scalar array `s` consists of five consecutive memory locations numbered from zero, and the index refers to an offset in the storage that's allocated for the array. However, the associative array `a` has no predefined size and doesn't store elements in consecutive memory locations. In addition, associative array keys have no relationship to the corresponding value storage location. You can access associative array elements `a[0]` and `a[-5]` and only two words of storage are allocated by DTrace. Furthermore, these elements don't have to be consecutive. Associative array keys are abstract names for the corresponding values and have no relationship to the value storage locations.

    If you create an array using an initial assignment and use a single integer expression as the array index , for example, `a[0] = 2`, the D compiler always creates a new associative array, even though in this expression `a` could also be interpreted as an assignment to a scalar array. Scalar arrays must be predeclared in this situation so that the D compiler can recognize the definition of the array size and infer that the array is a scalar array.

-   **Multidimensional Scalar Arrays**

    Multidimensional scalar arrays are used infrequently in D, but are provided for compatibility with ANSI C and are for observing and accessing OS data structures that are created by using this capability in C. A multidimensional array is declared as a consecutive series of scalar array sizes within square brackets `[]` following the base type. For example, to declare a fixed-size, two-dimensional array of integers of dimensions that's 12 rows by 34 columns, you would write the following declaration:

    ```
    int s[12][34];
    ```

    A multidimensional scalar array is accessed by using similar notation. For example, to access the value stored at row `0` and column `1`, you would write the D expression as follows:

    ```
    s[0][1]
    ```

    Storage locations for multidimensional scalar array values are computed by multiplying the row number by the total number of columns declared and then adding the column number.

    Be careful not to confuse the multidimensional array syntax with the D syntax for associative array accesses, that's, `s[0][1]`, isn't the same as `s[0,1]`\). If you use an incompatible key expression with an associative array or try an associative array access of a scalar array, the D compiler reports an appropriate error message and refuses to compile the program.


**Parent topic:**[D Program Syntax Reference](../reference/d_program_syntax_reference.md)

## Variable Scope <a id="dt_vscope_dlang">

Variable scoping is used to define where variable names are valid within a program and to avoid variable naming collisions. By using scoped variables you can control the availability of the variable instance to the whole program, a particular thread, or a specific clause.

The following table lists and describes the three primary variable scopes that are available. Note that external variables provide a fourth scope that falls outside of the control of the D program.

<table><thead><tr><th>

Scope

</th><th>

Syntax

</th><th>

Initial Value

</th><th>

Thread-safe?

</th><th>

Description

</th></tr></thead><tbody><tr><td>

global

</td><td>

`*myname*`

</td><td>

0

</td><td>

No

</td><td>

Any probe that fires on any thread accesses the same instance of the variable.

</td></tr><tr><td>

Thread-local

</td><td>

`self->*myname*`

</td><td>

0

</td><td>

Yes

</td><td>

Any probe that fires on a thread accesses the thread-specific instance of the variable.

</td></tr><tr><td>

Clause-local

</td><td>

`this->*myname*`

</td><td>

Not defined

</td><td>

Yes

</td><td>

Any probe that fires accesses an instance of the variable specific to that particular firing of the probe.

</td></tr><tbody></table>

**Note:**

Note the following information:

-   Scalar variables and associative arrays have a global scope and aren't multi-processor safe \(MP-safe\). Because the value of such variables can be changed by more than one processor, a variable can become corrupted if more than one probe changes it.

-   Aggregations are MP-safe even though they have a global scope because independent copies are updated locally before a final aggregation produces the global result.


### Global Variables

Global variables are used to declare variable storage that's persistent across the entire D program. Global variables provide the broadest scope.

Global variables of any type can be defined in a D program, including associative arrays. The following are some example global variable definitions:

```
x = 123; /* integer value */
s = "hello"; /* string value */
a[123, 'a'] = 456; /* associative array */
```

Global variables are created automatically on their first assignment and use the type appropriate for the right side of the first assignment statement. Except for scalar arrays, you don't need to explicitly declare global variables before using them. To create a declaration anyway, you must place it outside of program clauses, for example:

```
int x; /* declare int x as a global variable */
int x[unsigned long long, char];
syscall::read:entry
{
  x = 123;
  a[123, 'a'] = 456;
}
```

D variable declarations can't assign initial values. You can use a `BEGIN` probe clause to assign any initial values. All global variable storage is filled with zeroes by DTrace before you first reference the variable.

### Thread-Local Variables

Thread-local variables are used to declare variable storage that's local to each OS thread. Thread-local variables are useful in situations where you want to enable a probe and mark every thread that fires the probe with some tag or other data.

Thread-local variables are referenced by applying the `->` operator to the special identifier `self`, for example:

```
syscall::read:entry
{
  self->read = 1;
}
```

This D fragment example enables the probe on the `read()` system call and associates a thread-local variable named `read` with each thread that fires the probe. Similar to global variables, thread-local variables are created automatically on their first assignment and assume the type that's used on the right-hand side of the first assignment statement, which is `int` in this example.

Each time the `self->read` variable is referenced in the D program, the data object that's referenced is the one associated with the OS thread that was executing when the corresponding DTrace probe fired. You can think of a thread-local variable as an associative array that's implicitly indexed by a tuple that describes the thread's identity in the system. A thread's identity is unique over the lifetime of the system: if the thread exits and the same OS data structure is used to create a thread, this thread doesn't reuse the same DTrace thread-local storage identity.

When you have defined a thread-local variable, you can reference it for any thread in the system, even if the variable in question hasn't been previously assigned for that particular thread. If a thread's copy of the thread-local variable hasn't yet been assigned, the data storage for the copy is defined to be filled with zeroes. As with associative array elements, underlying storage isn't allocated for a thread-local variable until a non-zero value is assigned to it. Also, as with associative array elements, assigning zero to a thread-local variable causes DTrace to deallocate the underlying storage. Always assign zero to thread-local variables that are no longer in use.

Thread-local variables of any type can be defined in a D program, including associative arrays. The following are some example thread-local variable definitions:

```
self->x = 123; /* integer value */
self->s = "hello"; /* string value */
self->a[123, 'a'] = 456; /* associative array */
```

You don't need to explicitly declare thread-local variables before using them. To create a declaration anyway, you must place it outside of program clauses by prepending the keyword `self`, for example:

```
self int x; /* declare int x as a thread-local variable */ 
syscall::read:entry
{
  self->x = 123;
}
```

Thread-local variables are kept in a separate namespace from global variables so that you can reuse names. Remember that `x` and `self->x` aren't the same variable if you overload names in a program.

### Clause-Local Variables

Clause-local variable are used to restrict the storage of a variable to the particular firing of a probe. Clause-local is the narrowest scope. When a probe fires on a CPU, the D script is run in program order. Each clause-local variable is instantiated with an undefined value the first time it is used in the script. The same instance of the variable is used in all clauses until the D script has completed running for that particular firing of the probe.

Clause-local variables can be referenced and assigned by prefixing with `this->`:

```
BEGIN
{
  this->secs = timestamp / 1000000000;
  ...
}
```

To declare a clause-local variable explicitly before using it, you can do so by using the `this` keyword:

```
this int x;  /* an integer clause-local variable */
this char c; /* a character clause-local variable */

BEGIN
{
  this->x = 123;
  this->c = 'D';
}
```

Note that if a program contains several clauses for a single probe, any clause-local variables remain intact as the clauses are run sequentially and clause-local variables are persistent across different clauses that are enabling the same probe. While clause-local variables are persistent across clauses that are enabling the same probe, their values are undefined in the first clause processed for a specified probe. To avoid unexpected results, assign each clause-local variable an appropriate value before using it.

Clause-local variables can be defined using any scalar variable type, but associative arrays can't be defined using clause-local scope. The scope of clause-local variables only applies to the corresponding variable data, not to the name and type identity defined for the variable. When a clause-local variable is defined, this name and type signature can be used in any later D program clause.

You can use clause-local variables to accumulate intermediate results of calculations or as temporary copies of other variables. Access to a clause-local variable is much faster than access to an associative array. Therefore, if you need to reference an associative array value several times in the same D program clause, it's more efficient to copy it into a clause-local variable first and then reference the local variable repeatedly.

### External Variables

The D language uses the back quote character \(```\) as a special scoping operator for accessing symbols or variables that are defined in the OS, outside of the D program itself.

DTrace instrumentation runs inside the Linux kernel. So, in addition to accessing special DTrace variables and probe arguments, you can also access kernel data structures, symbols, and types. These capabilities enable advanced DTrace users, administrators, service personnel, and driver developers to examine low-level behavior of the OS kernel and device drivers.

For example, the Linux kernel contains a C declaration of a system variable named `max_pfn`. This variable is declared in C in the kernel source code as follows:

```
unsigned long max_pfn
```

To trace the value of this variable in a D program, you can write the following D statement:

```
trace(`max_pfn);
```

DTrace associates each kernel symbol with the type that's used for the symbol in the corresponding OS C code, which provides source-based access to the local OS data structures.

Kernel symbol names are kept in a separate namespace from D variable and function identifiers, so you don't need to be concerned about these names conflicting with other D variables. When you prefix a variable with a back quote, the D compiler searches the known kernel symbols and uses the list of loaded modules to find a matching variable definition. Because the Linux kernel can dynamically load modules with separate symbol namespaces, the same variable name might be used more than once in the active OS kernel. You can resolve these name conflicts by specifying the name of the kernel module that contains the variable to be accessed before the back quote in the symbol name. For example, you would refer to the address of the `_bar` function that's provided by a kernel module named `foo` as follows:

```
foo`_bar
```

You can apply any of the D operators to external variables, except for those that modify values, subject to the usual rules for operand types. When required, the D compiler loads the variable names that correspond to active kernel modules, so you don't need to declare these variables. You can't apply any operator to an external variable that modifies its value, such as `=` or `+=`. For safety reasons, DTrace prevents you from damaging or corrupting the state of the software that you're observing.

When you access external variables from a D program, you're accessing the internal implementation details of another program, such as the OS kernel or its device drivers. These implementation details don't form a stable interface upon which you can rely. Any D programs you write that depend on these details might not work when you next upgrade the corresponding piece of software. For this reason, external variables are typically used to debug performance or functionality problems by using DTrace.

