
# Pointers {#dt_ptrarr_dlang}

Pointers are memory addresses of data objects and reference memory used by the OS, by the user program, or by the D script. Pointers in D are data objects that store an integer virtual address value and associate it with a D type that describes the format of the data stored at the corresponding memory location.

You can explicitly declare a D variable to be of pointer type by first specifying the type of the referenced data and then appending an asterisk \(`*`\) to the type name. Doing so indicates you want to declare a pointer type, as shown in the following statement:

```
int *p;
```

The statement declares a D global variable named `p` that's a pointer to an integer. The declaration means that `p` is a 64-bit integer with a value that's the address of another integer located somewhere in memory. Because the compiled form of the D code is run at probe firing time inside the kernel itself, D pointers are typically pointers associated with the kernel's address space.

To create a pointer to a data object inside the kernel, you can compute its address by using the `&` operator. For example, the kernel source code declares an `unsigned long max_pfn` variable. You could trace the address of this variable by tracing the result of applying the `&` operator to the name of that object in D:

```
trace(&`max_pfn);
```

The `*` operator can be used to specify the object addressed by the pointer, and acts as the inverse of the `&` operator. For example, the following two D code fragments are equivalent in meaning:

```
q = &`max_pfn; trace(*q);

trace(`max_pfn); 
```

In this example, the first fragment creates a D global variable pointer `q`. Because the `max_pfn` object is of type `unsigned long`, the type of `&`max_pfn` is `unsigned long *`, a pointer to `unsigned long`. The type of `q` is implicit in the declaration. Tracing the value of `*q` follows the pointer back to the data object `max_pfn`. This fragment is therefore the same as the second fragment, which directly traces the value of the data object by using its name.

**Parent topic:**[D Program Syntax Reference](../reference/d_program_syntax_reference.md)

## Pointer Safety {#dt_ptrsafety_dlang}

DTrace is a robust, safe environment for running D programs. You might write a buggy D program, but invalid D pointer accesses don't cause DTrace or the OS kernel to fail or crash in any way. Instead, the DTrace software detects any invalid pointer accesses, and returns a `BADADDR` fault; the current clause execution quits, an ERROR probe fires, and tracing continues unless the program called `[exit](function_exit.md)` for the ERROR probe.

Pointers are required in D because they're an intrinsic part of the OS's implementation in C, but DTrace implements the same kind of safety mechanisms that are found in the Java programming language to prevent buggy programs from affecting themselves or each other. DTrace's error reporting is similar to the runtime environment for the Java programming language that detects a programming error and reports an exception.

To observe DTrace's error handling and reporting, you could write a deliberately bad D program using pointers. For example, in an editor, type the following D program and save it in a file named `badptr.d`:

```
BEGIN
{
  x = (int *)NULL;
  y = *x;
  trace(y);
}
```

The `badptr.d` program uses a cast expression to convert `NULL` to be a pointer to an integer. The program then dereferences the pointer by using the expression `*x`, assigns the result to another variable `y`, and then tries to trace `y`. When the D program is run, DTrace detects an invalid pointer access when the statement `y = *x` is processed and reports the following error:

```
dtrace: script '/tmp/badptr.d' matched 1 probe
dtrace: error on enabled probe ID 2 (ID 1: dtrace:::BEGIN): invalid address (0x0) in action #1 at BPF pc 156
```

Notice that the D program moves past the error and continues to run; the system and all observed processes remain unperturbed. You can also add an `ERROR` probe to any script to handle D errors. For details about the DTrace error mechanism, see [ERROR Probe](dtrace_providers_dtrace.md#).

## Pointer and Array Relationship {#dt_ptrarel_dlang}

A scalar array is represented by a variable that's associated with the address of its first storage location. A pointer is also the address of a storage location with a defined type. Thus, D permits the use of the array `[]` index notation with both pointer variables and array variables. For example, the following two D fragments are equivalent in meaning:

```
p = &a[0]; trace(p[2]);

trace(a[2]); 
```

In the first fragment, the pointer `p` is assigned to the address of the first element in scalar array `a` by applying the `&` operator to the expression `a[0]`. The expression `p[2]` traces the value of the third array element \(index 2\). Because `p` now contains the same address associated with `a`, this expression yields the same value as `a[2]`, shown in the second fragment. One consequence of this equivalence is that D permits you to access any index of any pointer or array. If you access memory beyond the end of a scalar array's predefined size, you either get an unexpected result or DTrace reports an invalid address error.

The difference between pointers and arrays is that a pointer variable refers to a separate piece of storage that contains the integer address of some other storage; whereas, an array variable names the array storage itself, not the location of an integer that in turn contains the location of the array.

This difference is manifested in the D syntax if you try to assign pointers and scalar arrays. If `x` and `y` are pointer variables, the expression `x = y` is legal; it copies the pointer address in `y` to the storage location that's named by `x`. If `x` and `y` are scalar array variables, the expression `x = y` isn't legal. Arrays can't be assigned as a whole in D. If `p` is a pointer and `a` is a scalar array, the statement `p = a` is permitted. This statement is equivalent to the statement `p = &a[0]`.

## Pointer Arithmetic {#dt_ptrarith_dlang}

As in C, pointer arithmetic in D isn't identical to integer arithmetic. Pointer arithmetic implicitly adjusts the underlying address by multiplying or dividing the operands by the size of the type referenced by the pointer.

The following D fragment illustrates this property:

```
int *x;

BEGIN
{
  trace(x);
  trace(x + 1);
  trace(x + 2);
}
```

This fragment creates an integer pointer `x` and then traces its value, its value incremented by one, and its value incremented by two. If you create and run this program, DTrace reports the integer values `0`, `4`, and `8`.

Because `x` is a pointer to an `int` \(size 4 bytes\), incrementing `x` adds 4 to the underlying pointer value. This property is useful when using pointers to reference consecutive storage locations such as arrays. For example, if `x` was assigned to the address of an array `a`, the expression `x + 1` would be equivalent to the expression `&a[1]`. Similarly, the expression `*(x + 1)` would reference the value `a[1]`. Pointer arithmetic is implemented by the D compiler whenever a pointer value is incremented by using the `+`, `++`, or `=+` operators. Pointer arithmetic is also applied as follows; when an integer is subtracted from a pointer on the left-hand side, when a pointer is subtracted from another pointer, or when the `--` operator is applied to a pointer.

For example, the following D program would trace the result `2`:

```
int *x, *y;
int a[5];

BEGIN
{
  x = &a[0];
  y = &a[2];
  trace(y - x);
}
```

## Generic Pointers {#dt_genptr_dlang}

Sometimes it's useful to represent or manipulate a generic pointer address in a D program without specifying the type of data referred to by the pointer. Generic pointers can be specified by using the type `void *`, where the keyword `void` represents the absence of specific type information, or by using the built-in type alias `uintptr_t`, which is aliased to an unsigned integer type of size that's appropriate for a pointer in the current data model. You can't apply pointer arithmetic to an object of type `void *`, and these pointers can't be dereferenced without casting them to another type first. You can cast a pointer to the `uintptr_t` type when you need to perform integer arithmetic on the pointer value.

Pointers to `void` can be used in any context where a pointer to another data type is required, such as an associative array tuple expression or the right-hand side of an assignment statement. Similarly, a pointer to any data type can be used in a context where a pointer to `void` is required. To use a pointer to a non-`void` type in place of another non-`void` pointer type, an explicit cast is required. You must always use explicit casts to convert pointers to integer types, such as `uintptr_t`, or to convert these integers back to the appropriate pointer type.

## Pointers to DTrace Objects {#dt_ptrobj_dlang}

The D compiler prohibits you from using the `&` operator to obtain pointers to DTrace objects such as associative arrays, built-in functions, and variables. You're prohibited from obtaining the address of these variables so that the DTrace runtime environment is free to relocate them as needed between probe firings . In this way, DTrace can more efficiently manage the memory required for programs. If you create composite structures, it's possible to construct expressions that retrieve the kernel address of DTrace object storage. Avoid creating such expressions in D programs. If you need to use such an expression, don't rely on the address being the same across probe firings.

## Pointers and Address Spaces {#dt_ptraddrsp_dlang}

A pointer is an address that provides a translation within some *virtual address space* to a piece of physical memory. DTrace runs D programs within the address space of the OS kernel itself. The Linux system manages many address spaces: one for the OS kernel itself, and one for each user process. Because each address space provides the illusion that it can access all the memory on the system, the same virtual address pointer value can be reused across address spaces, but translate to different physical memory. Therefore, when writing D programs that use pointers, you must be aware of the address space corresponding to the pointers you intend to use.

For example, if you use the `syscall` provider to instrument entry to a system call that takes a pointer to an integer or array of integers as an argument, such as, `pipe()`, it would not be valid to dereference that pointer or array using the `*` or `[]` operators because the address in question is an address in the address space of the user process that performed the system call. Applying the `*` or `[]` operators to this address in D would result in kernel address space access, which would result in an invalid address error or in returning unexpected data to the D program, depending on whether the address happened to match a valid kernel address.

To access user-process memory from a DTrace probe, you must apply one of the `[copyin](function_copyin.md)`, `[copyinstr](function_copyinstr.md)`, or `[copyinto](function_copyinto.md)` functions. To avoid confusion, take care when writing D programs to name and comment variables storing user addresses appropriately. You can also store user addresses as `uintptr_t` so that you don't accidentally compile D code that dereferences them..

