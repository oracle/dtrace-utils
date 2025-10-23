
# DTrace String Processing <a id="dt_strings_dlang">

DTrace provides facilities for tracing and manipulating strings. This section describes the complete set of D language features for declaring and manipulating strings. Unlike ANSI C, strings in D have their own built-in type and operator support to enable you to easily and unambiguously use them in tracing programs.

**Parent topic:**[D Program Syntax Reference](../reference/d_program_syntax_reference.md)

## String Representation <a id="dt_strrep_dlang">

In DTrace, strings are represented as an array of characters ending in a null byte, which is a byte with a value of zero, usually written as `'\0'`. The visible part of the string is of variable length, depending on the location of the null byte, but DTrace stores each string in a fixed-size array so that each probe traces a consistent amount of data. Strings cannot exceed the length of the predefined string limit. However, the limit can be modified in your D program or on the `dtrace` command line by tuning the `strsize` option. The default string limit is 256 bytes.

The D language provides an explicit `string` type rather than using the type `char *` to refer to strings. The string type is equivalent to `char *`, in that it's the address of a sequence of characters, but the D compiler and D functions such as `trace` provide enhanced capabilities when applied to expressions of type string. For example, the string type removes the ambiguity of type `char *` when you need to trace the actual bytes of a string.

In the following D statement, if `s` is of type `char *`, DTrace traces the value of the pointer `s`, which means it traces an integer address value:

```
trace(s);
```

In the following D statement, by the definition of the `*` operator, the D compiler dereferences the pointer `s` and traces the single character at that location:

```
trace(*s);
```

These behaviors enable you to manipulate character pointers that refer to either single characters, or to arrays of byte-sized integers that aren't strings and don't end with a null byte.

In the next D statement, if `s` is of type `string`, the string type indicates to the D compiler that you want DTrace to trace a null terminated string of characters whose address is stored in the variable `s`:

```
trace(s);
```

You can also perform lexical comparison of expressions of type string. See [String Comparison](dtrace-ref-DTraceSupportforStrings.md).

## String Constants <a id="dt_strcon_dlang">

String constants are enclosed in pairs of double quotes \(`""`\) and are automatically assigned the type `string` by the D compiler. You can define string constants of any length, limited only by the amount of memory DTrace is permitted to consume on the system and by whatever limit you have set for the `strsize` DTrace runtime option. The terminating null byte \(`\0`\) is added automatically by the D compiler to any string constants that you declare. The size of a string constant object is the number of bytes associated with the string, plus one additional byte for the terminating null byte.

A string constant can't contain a literal newline character. To create strings containing newlines, use the `\n` escape sequence instead of a literal newline. String constants can also contain any of the special character escape sequences that are defined for character constants.

## String Assignment <a id="dt_strasg_dlang">

Unlike the assignment of `char *` variables, strings are copied by value and not by reference. The string assignment operator `=` copies the actual bytes of the string from the source operand up to and including the null byte to the variable on the left-hand side, which must be of type `string`.

You can use a declaration to create a string variable:

```
string s;
```

Or you can create a string variable by assigning it an expression of type `string`.

For example, the D statement:

```
s = "hello";
```

creates a variable `s` of type `string` and copies the six bytes of the string `"hello"` into it \(five printable characters, plus the null byte\).

String assignment is analogous to the C library function `strcpy()`, with the exception that if the source string exceeds the limit of the storage of the destination string, the resulting string is automatically truncated by a null byte at this limit.

You can also assign to a string variable an expression of a type that's compatible with strings. In this case, the D compiler automatically promotes the source expression to the string type and performs a string assignment. The D compiler permits any expression of type `char *` or of type `char[n]`, a scalar array of `char` of any size, to be promoted to a string.

## String Conversion <a id="dt_strconv_dlang">

Expressions of other types can be explicitly converted to type `string` by using a cast expression or by applying the special `stringof` operator, which are equivalent in the following meaning:

```
s = (string) *expression*;
s = stringof (*expression*);
```

The expression is interpreted as an address to the string.

The `stringof` operator binds very tightly to the operand on its right-hand side. You can optionally surround the expression by using parentheses, for clarity.

Scalar type expressions, such as a pointer or integer, or a scalar array address can be converted to strings, in that the scalar is interpreted as an address to a char type. Expressions of other types such as `void` may not be converted to `string`. If you erroneously convert an invalid address to a string, the DTrace safety features prevents you from damaging the system or DTrace, but you might end up tracing a sequence of undecipherable characters.

## String Comparison <a id="dt_strcomp_dlang">

D overloads the binary relational operators and permits them to be used for string comparisons, as well as integer comparisons.
The relational operators perform string comparison whenever both operands are of type `string`
or when one operand is of type `string` and the other operand can be promoted to type `string`.
See [String Assignment](dtrace-ref-DTraceSupportforStrings.md#dt_strasg_dlang) for a detailed description.
See also the following table, which lists the relational operators that can be used to compare strings.

| Operator | Description                                                       |
| :---     | :---                                                              |
| `<`      | Left-hand operand is less than right-operand.                     |
| `<=`     | Left-hand operand is less than or equal to right-hand operand.    |
| `>`      | Left-hand operand is greater than right-hand operand.             |
| `>=`     | Left-hand operand is greater than or equal to right-hand operand. |
| `==`     | Left-hand operand is equal to right-hand operand.                 |
| `!=`     | Left-hand operand is not equal to right-hand operand.             |

As with integers, each operator evaluates to a value of type `int`, which is equal to one if the condition is true or zero if it is false.

The relational operators compare the two input strings byte-by-byte, similarly to the C library routine `strcmp()`. Each byte is compared by using its corresponding integer value in the ASCII character set until a null byte is read or the maximum string length is reached. See the `ascii(7)` manual page for more information. Some example D string comparisons and their results are shown in the following table.

<table><thead><tr><th>

D string comparison

</th><th>

Result

</th></tr></thead><tbody><tr><td>

`"coffee" < "espresso"`

</td><td>

Returns 1 \(true\)

</td></tr><tr><td>

`"coffee" == "coffee"`

</td><td>

Returns 1 \(true\)

</td></tr><tr><td>

`"coffee"" >= "mocha"`

</td><td>

Returns 0 \(false\)

</td></tr><tbody></table>

**Note:**

Identical Unicode strings might compare as being different if one or the other of the strings isn't normalized.
