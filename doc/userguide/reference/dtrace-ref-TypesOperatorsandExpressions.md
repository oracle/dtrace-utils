
# Types, Operators, and Expressions <a id="dt_types_dlang">

D provides the ability to access and manipulate various data objects: variables and data structures can be created and changed, data objects that are defined in the OS kernel and user processes can be accessed, and integer, floating-point, and string constants can be declared. D provides a superset of the ANSI C operators that are used to manipulate objects and create complex expressions. This section describes the detailed set of rules for types, operators, and expressions.

**Parent topic:**[D Program Syntax Reference](../reference/d_program_syntax_reference.md)

## Identifier Names and Keywords <a id="dt_ident_dlang">

D identifier names are composed of uppercase and lowercase letters, digits, and underscores, where the first character must be a letter or underscore. All identifier names beginning with an underscore \(`_`\) are reserved for use by the D system libraries. Avoid using these names in D programs. By convention, D programmers typically use mixed-case names for variables and all uppercase names for constants.

D language keywords are special identifiers that are reserved for use in the programming language syntax itself. These names are always specified in lowercase and must not be used for the names of D variables. The following table lists the keywords that are reserved for use by the D language.

<table><tbody><tr><td>

`auto*`

</td><td>

`do*`

</td><td>

`if*`

</td><td>

`register*`

</td><td>

`string+`

</td><td>

`unsigned`

</td></tr><tr><td>

`break*`

</td><td>

`double`

</td><td>

`import*+`

</td><td>

`restrict*`

</td><td>

`stringof+`

</td><td>

`void`

</td></tr><tr><td>

`case*`

</td><td>

`else*`

</td><td>

`inline`

</td><td>

`return*`

</td><td>

`struct`

</td><td>

`volatile`

</td></tr><tr><td>

`char`

</td><td>

`enum`

</td><td>

`int`

</td><td>

`self+`

</td><td>

`switch*`

</td><td>

`while*`

</td></tr><tr><td>

`const`

</td><td>

`extern`

</td><td>

`long`

</td><td>

`short`

</td><td>

`this+`

</td><td>

`xlate+`

</td></tr><tr><td>

`continue*`

</td><td>

`float`

</td><td>

`offsetof+`

</td><td>

`signed`

</td><td>

`translator+`

</td><td>

 
</td></tr><tr><td>

`counter*+`

</td><td>

`for*`

</td><td>

`probe*+`

</td><td>

`sizeof`

</td><td>

`typedef`

</td><td>

 
</td></tr><tr><td>

`default*`

</td><td>

`goto*`

</td><td>

`provider*+`

</td><td>

`static*`

</td><td>

`union`

</td><td>

 
</td></tr><tbody></table>
D reserves for use as keywords a superset of the ANSI C keywords. The keywords reserved for future use by the D language are marked with `*`. The D compiler produces a syntax error if you try to use a keyword that's reserved for future use. The keywords that are defined by D but not defined by ANSI C are marked with `+`. D provides the complete set of types and operators found in ANSI C. The major difference in D programming is the absence of control-flow constructs. Note that keywords associated with control-flow in ANSI C are reserved for future use in D.

## Data Types and Sizes <a id="dt_dtypes_dlang">

D provides fundamental data types for integers and floating-point constants. Arithmetic can only be performed on integers in D programs. Floating-point constants can be used to initialize data structures, but floating-point arithmetic isn't permitted in D. D provides a 64-bit data model for use in writing programs.

The names of the integer types and their sizes in the 64-bit data model are shown in the following table. Integers are always represented in twos-complement form in the native byte-encoding order of a system.

<table><thead><tr><th>

Type Name

</th><th>

64-bit Size

</th></tr></thead><tbody><tr><td>

`char`

</td><td>

1 byte

</td></tr><tr><td>

`short`

</td><td>

2 bytes

</td></tr><tr><td>

`int`

</td><td>

4 bytes

</td></tr><tr><td>

`long`

</td><td>

8 bytes

</td></tr><tr><td>

`long long`

</td><td>

8 bytes

</td></tr><tbody></table>
Integer types, including `char`, can be prefixed with the signed or unsigned qualifier. Integers are implicitly signed unless the unsigned qualifier isn't specified. The D compiler also provides the type aliases that are listed in the following table.

<table><thead><tr><th>

Type Name

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`int8_t`

</td><td>

1-byte signed integer

</td></tr><tr><td>

`int16_t`

</td><td>

2-byte signed integer

</td></tr><tr><td>

`int32_t`

</td><td>

4-byte signed integer

</td></tr><tr><td>

`int64_t`

</td><td>

8-byte signed integer

</td></tr><tr><td>

`intptr_t`

</td><td>

Signed integer of size equal to a pointer

</td></tr><tr><td>

`uint8_t`

</td><td>

1-byte unsigned integer

</td></tr><tr><td>

`uint16_t`

</td><td>

2-byte unsigned integer

</td></tr><tr><td>

`uint32_t`

</td><td>

4-byte unsigned integer

</td></tr><tr><td>

`uint64_t`

</td><td>

8-byte unsigned integer

</td></tr><tr><td>

`uintptr_t`

</td><td>

Unsigned integer of size equal to a pointer

</td></tr><tbody></table>
These type aliases are equivalent to using the name of the corresponding base type listed in the previous table and are appropriately defined for each data model. For example, the `uint8_t` type name is an alias for the type unsigned `char`.

**Note:**

The predefined type aliases can't be used in files that are included by the preprocessor.

D provides floating-point types for compatibility with ANSI C declarations and types. Floating-point operators aren't available in D, but floating-point data objects can be traced and formatted with the `printf` function. You can use the floating-point types that are listed in the following table.

<table><thead><tr><th>

Type Name

</th><th>

64-bit Size

</th></tr></thead><tbody><tr><td>

`float`

</td><td>

4 bytes

</td></tr><tr><td>

`double`

</td><td>

8 bytes

</td></tr><tr><td>

`long double`

</td><td>

16 bytes

</td></tr><tbody></table>
D also provides the special type `string` to represent ASCII strings. Strings are discussed in more detail in [DTrace String Processing](dtrace-ref-DTraceSupportforStrings.md).

## Constants <a id="dt_consts_dlang">

Integer constants can be written in decimal \(`12345`\), octal \(`012345`\), or hexadecimal `(0x12345`\) format. Octal \(base 8\) constants must be prefixed with a leading zero. Hexadecimal \(base 16\) constants must be prefixed with either `0x` or `0X`. Integer constants are assigned the smallest type among `int`, `long`, and `long long` that can represent their value. If the value is negative, the signed version of the type is used. If the value is positive and too large to fit in the signed type representation, the unsigned type representation is used. You can apply one of the suffixes listed in the following table to any integer constant to explicitly specify its D type.

<table><thead><tr><th>

Suffix

</th><th>

D type

</th></tr></thead><tbody><tr><td>

`u` or `U`

</td><td>

`unsigned` version of the type selected by the compiler

</td></tr><tr><td>

`l` or `L`

</td><td>

`long`

</td></tr><tr><td>

`ul` or `UL`

</td><td>

`unsigned long`

</td></tr><tr><td>

`ll` or `LL`

</td><td>

`long long`

</td></tr><tr><td>

`ull` or `ULL`

</td><td>

`unsigned long long`

</td></tr><tbody></table>
Floating-point constants are always written in decimal format and must contain either a decimal point \(`12.345`\), an exponent \(`123e45`\), or both \(`123.34e-5`\). Floating-point constants are assigned the type `double` by default. You can apply one of the suffixes listed in the following table to any floating-point constant to explicitly specify its D type.

<table><thead><tr><th>

Suffix

</th><th>

D type

</th></tr></thead><tbody><tr><td>

`f` or `F`

</td><td>

`float`

</td></tr><tr><td>

`l` or `L`

</td><td>

`long double`

</td></tr><tbody></table>
Character constants are written as a single character or escape sequence that's inside a pair of single quotes \(`'a'`\). Character constants are assigned the `int` type rather than `char` and are equivalent to an integer constant with a value that's determined by that character's value in the ASCII character set. See the `ascii(7)` manual page for a list of characters and their values. You can also use any of the special escape sequences that are listed in the following table. D uses the same escape sequences as those found in ANSI C.

**Table:**  Character Escape Sequences<a id="char_esc_seqs">

| Escape Sequence | Represents      |     | Escape Sequence | Represents               |
| :---            | :---            | --- | :---            | :---                     |
| `\a`            | alert           |     | `\\`            | backslash                |
| `\b`            | backspace       |     | `\?`            | question mark            |
| `\f`            | form feed       |     | `\'`            | single quote             |
| `\n`            | newline         |     | `\"`            | double quote             |
| `\r`            | carriage return |     | `\0`*oo*        | octal value 0*oo*        |
| `\t`            | horizontal tab  |     | `\x`*hh*        | hexadecimal value 0x*hh* |
| `\v`            | vertical tab    |     | `\0`            | null character           |

You can include more than one character specifier inside single quotes to create integers with individual bytes that are initialized according to the corresponding character specifiers. The bytes are read left-to-right from a character constant and assigned to the resulting integer in the order corresponding to the native endianness of the operating environment. Up to eight character specifiers can be included in a single character constant.

Strings constants of any length can be composed by enclosing them in a pair of double quotes \(`"hello"`\). A string constant can't contain a literal newline character. To create strings containing newlines, use the `\n` escape sequence instead of a literal newline. String constants can contain any of the special character escape sequences that are shown for character constants before. Similar to ANSI C, strings are represented as arrays of characters that end with a null character \(`\0`\) that's implicitly added to each string constant you declare. String constants are assigned the special D type `string`. The D compiler provides a set of special features for comparing and tracing character arrays that are declared as strings.

## Arithmetic Operators <a id="dt_arithops_dlang">

Binary arithmetic operators are described in the following table. These operators all have the same meaning for integers that they do in ANSI C.

<table><thead><tr><th>

Operator

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`+`

</td><td>

Integer addition

</td></tr><tr><td>

`-`

</td><td>

Integer subtraction

</td></tr><tr><td>

`*`

</td><td>

Integer multiplication

</td></tr><tr><td>

`/`

</td><td>

Integer division

</td></tr><tr><td>

`%`

</td><td>

Integer modulus

</td></tr><tbody></table>
Arithmetic in D can only be performed on integer operands or on pointers. Arithmetic can't be performed on floating-point operands in D programs. The DTrace execution environment doesn't take any action on integer overflow or underflow. You must check for these conditions in situations where overflow and underflow can occur.

However, the DTrace execution environment does automatically check for and report division by zero errors resulting from improper use of the `/` and `%` operators. If a D program contains an invalid division operation that's detectable at compile time, a compile error is returned and the compilation fails. If the invalid division operation takes place at run time, processing of the current clause is quit, and the `ERROR` probe is activated. If the D program has no clause for the `ERROR` probe, the error is printed and tracing continues. Otherwise, the actions in the clause assigned to the `ERROR` probe are processed. Errors that are detected by DTrace have no effect on other DTrace users or on the OS kernel. You therefore don't need to be concerned about causing any damage if a D program inadvertently contains one of these errors.

In addition to these binary operators, the `+` and `-` operators can also be used as unary operators, and these operators have higher precedence than any of the binary arithmetic operators. The order of precedence and associativity properties for all D operators is presented in [Operator Precedence](dtrace-ref-TypesOperatorsandExpressions.md). You can control precedence by grouping expressions in parentheses \(`()`\).

## Relational Operators <a id="dt_relatops_dlang">

Binary relational operators are described in the following table. These operators all have the same meaning that they do in ANSI C.

<table><thead><tr><th>

Operator

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`<`

</td><td>

Left-hand operand is less than right-operand

</td></tr><tr><td>

`<=`

</td><td>

Left-hand operand is less than or equal to right-hand operand

</td></tr><tr><td>

`>`

</td><td>

Left-hand operand is greater than right-hand operand

</td></tr><tr><td>

`>=`

</td><td>

Left-hand operand is greater than or equal to right-hand operand

</td></tr><tr><td>

`==`

</td><td>

Left-hand operand is equal to right-hand operand

</td></tr><tr><td>

`!=`

</td><td>

Left-hand operand isn't equal to right-hand operand

</td></tr><tbody></table>
Relational operators are most often used to write D predicates. Each operator evaluates to a value of type `int`, which is equal to one if the condition is `true`, or zero if it's `false`.

Relational operators can be applied to pairs of integers, pointers, or strings. If pointers are compared, the result is equivalent to an integer comparison of the two pointers interpreted as unsigned integers. If strings are compared, the result is determined as if by performing a `strcmp()` on the two operands. The following table shows some example D string comparisons and their results.

<table><thead><tr><th>

D string comparison

</th><th>

Result

</th></tr></thead><tbody><tr><td>

`"coffee" < "espresso"`

</td><td>

Returns 1 \(`true`\)

</td></tr><tr><td>

`"coffee" == "coffee"`

</td><td>

Returns 1 \(`true`\)

</td></tr><tr><td>

`"coffee"" >= "mocha"`

</td><td>

Returns 0 \(`false`\)

</td></tr><tbody></table>
Relational operators can also be used to compare a data object associated with an enumeration type with any of the enumerator tags defined by the enumeration.

## Logical Operators <a id="dt_logicops_dlang">

Binary logical operators are listed in the following table. The first two operators are equivalent to the corresponding ANSI C operators.

<table><thead><tr><th>

Operator

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`&&`

</td><td>

Logical `AND`: true if both operands are true

</td></tr><tr><td>

`||`

</td><td>

Logical `OR`: true if one or both operands are true

</td></tr><tr><td>

`^^`

</td><td>

Logical `XOR`: true if exactly one operand is true

</td></tr><tbody></table>
Logical operators are most often used in writing D predicates. The logical `AND` operator performs the following short-circuit evaluation: if the left-hand operand is false, the right-hand expression isn't evaluated. The logical `OR` operator also performs the following short-circuit evaluation: if the left-hand operand is true, the right-hand expression isn't evaluated. The logical `XOR` operator doesn't short-circuit. Both expression operands are always evaluated.

In addition to the binary logical operators, the unary `!` operator can be used to perform a logical negation of a single operand: it converts a zero operand into a one and a non-zero operand into a zero. By convention, D programmers use `!` when working with integers that are meant to represent Boolean values and `== 0` when working with non-Boolean integers, although the expressions are equivalent.

The logical operators can be applied to operands of integer or pointer types. The logical operators interpret pointer operands as unsigned integer values. As with all logical and relational operators in D, operands are true if they have a non-zero integer value and false if they have a zero integer value.

## Bitwise Operators <a id="dt_bitwiseops_dlang">

D provides the bitwise operators that are listed in the following table for manipulating individual bits inside integer operands. These operators all have the same meaning as in ANSI C.

<table><thead><tr><th>

Operator

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`~`

</td><td>

Unary operator that can be used to perform a bitwise negation of a single operand: it converts each zero bit in the operand into a one bit, and each one bit in the operand into a zero bit

</td></tr><tr><td>

`&`

</td><td>

Bitwise `AND`

</td></tr><tr><td>

`|`

</td><td>

Bitwise `OR`

</td></tr><tr><td>

`^`

</td><td>

Bitwise `XOR`

</td></tr><tr><td>

`<<`

</td><td>

Shift the left-hand operand left by the number of bits specified by the right-hand operand

</td></tr><tr><td>

`>>`

</td><td>

Shift the left-hand operand right by the number of bits specified by the right-hand operand

</td></tr><tbody></table>
The shift operators are used to move bits left or right in a particular integer operand. Shifting left fills empty bit positions on the right-hand side of the result with zeroes. Shifting right using an unsigned integer operand fills empty bit positions on the left-hand side of the result with zeroes. Shifting right using a signed integer operand fills empty bit positions on the left-hand side with the value of the sign bit, also known as an *arithmetic shift* operation.

Shifting an integer value by a negative number of bits or by a number of bits larger than the number of bits in the left-hand operand itself produces an undefined result. The D compiler produces an error message if the compiler can detect this condition when you compile the D program.

## Assignment Operators <a id="dt_assignops_dlang">

Binary assignment operators are listed in the following table. You can only modify D variables and arrays. Kernel data objects and constants can not be modified using the D assignment operators. The assignment operators have the same meaning as they do in ANSI C.

<table><thead><tr><th>

Operator

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`=`

</td><td>

Set the left-hand operand equal to the right-hand expression value.

</td></tr><tr><td>

`+=`

</td><td>

Increment the left-hand operand by the right-hand expression value

</td></tr><tr><td>

`-=`

</td><td>

Decrement the left-hand operand by the right-hand expression value.

</td></tr><tr><td>

`*=`

</td><td>

Multiply the left-hand operand by the right-hand expression value.

</td></tr><tr><td>

`/=`

</td><td>

Divide the left-hand operand by the right-hand expression value.

</td></tr><tr><td>

`%=`

</td><td>

Modulo the left-hand operand by the right-hand expression value.

</td></tr><tr><td>

`|=`

</td><td>

Bitwise OR the left-hand operand with the right-hand expression value.

</td></tr><tr><td>

`&=`

</td><td>

Bitwise AND the left-hand operand with the right-hand expression value.

</td></tr><tr><td>

`^=`

</td><td>

Bitwise XOR the left-hand operand with the right-hand expression value.

</td></tr><tr><td>

`<<=`

</td><td>

Shift the left-hand operand left by the number of bits specified by the right-hand expression value.

</td></tr><tr><td>

`>>=`

</td><td>

Shift the left-hand operand right by the number of bits specified by the right-hand expression value.

</td></tr><tbody></table>
Aside from the assignment operator `=`, the other assignment operators are provided as shorthand for using the `=` operator with one of the other operators that were described earlier. For example, the expression `x = x + 1` is equivalent to the expression `x += 1`. These assignment operators adhere to the same rules for operand types as the binary forms described earlier.

The result of any assignment operator is an expression equal to the new value of the left-hand expression. You can use the assignment operators or any of the operators described thus far in combination to form expressions of arbitrary complexity. You can use parentheses `()` to group terms in complex expressions.

## Increment and Decrement Operators <a id="dt_incdecops_dlang">

D provides the special unary `++` and `--` operators for incrementing and decrementing pointers and integers. These operators have the same meaning as they do in ANSI C. These operators can be applied to variables and to the individual elements of a struct, union, or array. The operators can be applied either before or after the variable name. If the operator appears before the variable name, the variable is first changed and then the resulting expression is equal to the new value of the variable. For example, the following two code fragments produce identical results:

```
x += 1; y = x;

y = ++x;
```

If the operator appears after the variable name, then the variable is changed after its current value is returned for use in the expression. For example, the following two code fragments produce identical results:

```
y = x; x -= 1;

y = x--;
```

You can use the increment and decrement operators to create new variables without declaring them. If a variable declaration is omitted and the increment or decrement operator is applied to a variable, the variable is implicitly declared to be of type `int64_t`.

To use the increment and decrement operators on elements of an array or struct, place the operator after or before the full reference to the element:

```
int foo[5];
struct { int a; } bar;

bar.a++;
foo[1]++;
--foo[1];
```

The increment and decrement operators can be applied to integer or pointer variables. When applied to integer variables, the operators increment, or decrement the corresponding value by one. When applied to pointer variables, the operators increment, or decrement the pointer address by the size of the data type that's referenced by the pointer.

## Conditional Expressions <a id="dt_condexp_dlang">

D doesn't provide the facility to use `if-then-else` constructs. Instead, conditional expressions, by using the ternary operator \(`?:`\), can be used to approximate some of this functionality. The ternary operator associates a triplet of expressions, where the first expression is used to conditionally evaluate one of the other two.

For example, the following D statement could be used to set a variable `x` to one of two strings, depending on the value of `i`:

```
x = i == 0 ? "zero" : "non-zero";
```

In the previous example, the expression `i == 0` is first evaluated to determine whether it's true or false. If the expression is true, the second expression is evaluated and its value is returned. If the expression is false, the third expression is evaluated and its value is returned.

As with any D operator, you can use several `?:` operators in a single expression to create more complex expressions. For example, the following expression would take a `char` variable `c` containing one of the characters `0-9`, `a-f`, or `A-F`, and return the value of this character when interpreted as a digit in a hexadecimal \(base 16\) integer:

```
hexval = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c + 10 - 'a' : c + 10 - 'A';
```

To be evaluated for its truth value, the first expression that's used with `?:` must be a pointer or integer. The second and third expressions can be of any compatible types. You can't construct a conditional expression where, for example, one path returns a string and another path returns an integer. The second and third expressions must be true expressions that have a value. Therefore, data reporting functions can't be used in these expressions because those functions don't return a value. To conditionally trace data, use a predicate instead.

## Type Conversions <a id="dt_typeconv_dlang">

When expressions are constructed by using operands of different but compatible types, type conversions are performed to determine the type of the resulting expression. The D rules for type conversions are the same as the arithmetic conversion rules for integers in ANSI C. These rules are sometimes referred to as the *usual arithmetic conversions*.

Each integer type is ranked in the order `char`, `short`, `int`, `long`, `long long`, with the corresponding unsigned types assigned a rank higher than its signed equivalent, but below the next integer type. When you construct an expression using two integer operands such as `x + y` and the operands are of different integer types, the operand type with the highest rank is used as the result type.

If a conversion is required, the operand with the lower rank is first *promoted* to the type of the higher rank. Promotion doesn't change the value of the operand: it only extends the value to a larger container according to its sign. If an unsigned operand is promoted, the unused high-order bits of the resulting integer are filled with zeroes. If a signed operand is promoted, the unused high-order bits are filled by performing sign extension. If a signed type is converted to an unsigned type, the signed type is first sign-extended and then assigned the new, unsigned type that's determined by the conversion.

Integers and other types can also be explicitly *cast* from one type to another. Pointers and integers can be cast to any integer or pointer types, but not to other types.

An integer or pointer cast is formed using an expression such as the following:

```
y = (int)x;
```

In this example, the destination type is within parentheses and used to prefix the source expression. Integers are cast to types of higher rank by performing promotion. Integers are cast to types of lower rank by zeroing the excess high-order bits of the integer.

Because D doesn't include floating-point arithmetic, no floating-point operand conversion or casting is permitted and no rules for implicit floating-point conversion are defined.

## Operator Precedence <a id="dt_preced_dlang">

D includes complex rules for operator precedence and associativity. The rules provide precise compatibility with the ANSI C operator precedence rules. The entries in the following table are in order from highest precedence to lowest precedence.

<table><thead><tr><th>

Operators

</th><th>

Associativity

</th></tr></thead><tbody><tr><td>

`() [] -> .`

</td><td>

Left to right

</td></tr><tr><td>

`! ~ ++ -- + - * & (*type*) sizeof stringof offsetof xlate`

</td><td>

Right to left

 \(Note that these are the unary operators\)

</td></tr><tr><td>

`* / %`

</td><td>

Left to right

</td></tr><tr><td>

`+ -`

</td><td>

Left to right

</td></tr><tr><td>

`<< >>`

</td><td>

Left to right

</td></tr><tr><td>

`< <= > >=`

</td><td>

Left to right

</td></tr><tr><td>

`== !=`

</td><td>

Left to right

</td></tr><tr><td>

`&`

</td><td>

Left to right

</td></tr><tr><td>

`^`

</td><td>

Left to right

</td></tr><tr><td>

`|`

</td><td>

Left to right

</td></tr><tr><td>

`&&`

</td><td>

Left to right

</td></tr><tr><td>

`^^`

</td><td>

Left to right

</td></tr><tr><td>

`||`

</td><td>

Left to right

</td></tr><tr><td>

`?:`

</td><td>

Right to left

</td></tr><tr><td>

`= += -= *= /= %= &= ^= ?= <<= >>=`

</td><td>

Right to left

</td></tr><tr><td>

`,`

</td><td>

Left to right

</td></tr><tbody></table>
The comma \(`,`\) operator that's listed in the table is for compatibility with the ANSI C comma operator. It can be used to evaluate a set of expressions in left-to-right order and return the value of the right most expression. This operator is provided for compatibility with C and usage isn't recommended.

The `()` entry listed in the table of operator precedence represents a function call. A comma is also used in D to list arguments to functions and to form lists of associative array keys. Note that this comma isn't the same as the comma operator and doesn't guarantee left-to-right evaluation. The D compiler provides no guarantee regarding the order of evaluation of arguments to a function or keys to an associative array. Be careful of using expressions with interacting side-effects, such as the pair of expressions `i` and `i++`, in these contexts.

The `[]` entry listed in the table of operator precedence represents an array or associative array reference. Note that aggregations are also treated as associative arrays. The `[]` operator can also be used to index into fixed-size C arrays.

The following table provides further explanation for the function of several miscellaneous operators that are provided by the D language.

<table><thead><tr><th>

Operators

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`sizeof`

</td><td>

Computes the size of an object.

</td></tr><tr><td>

`offsetof`

</td><td>

Computes the offset of a type member.

</td></tr><tr><td>

`stringof`

</td><td>

Converts the operand to a string.

</td></tr><tr><td>

`xlate`

</td><td>

Translates a data type.

</td></tr><tr><td>

unary `&`

</td><td>

Computes the address of an object.

</td></tr><tr><td>

unary `*`

</td><td>

Dereferences a pointer to an object.

</td></tr><tr><td>

`->` and `.`

</td><td>

Accesses a member of a structure or union type.

</td></tr><tbody></table>

## Type and Constant Definitions <a id="dt_typcondef_dlang">

This section describes how to declare type aliases and named constants in D. It also discusses D type and namespace management for program and OS types and identifiers.

### typedefs <a id="dt_typedefs_dlang">

The `typedef` keyword is used to declare an identifier as an alias for an existing type. The `typedef` declaration is used outside of probe clauses in the following form:

```
typedef *existing-type* *new-type* ;
```

where *existing-type* is any type declaration and *new-type* is an identifier to be used as the alias for this type. For example, the D compiler uses the following declaration internally to create the `uint8_t` type alias:

```
typedef unsigned char uint8_t;
```

You can use type aliases anywhere that a normal type can be used, such as the type of a variable or associative array value or tuple member. You can also combine `typedef` with more elaborate declarations such as the definition of a new `struct`, as shown in the following example:

```
typedef struct foo {
  int x;
  int y;
} foo_t;
```

In the previous example, `struct foo` is defined using the same type as its alias, `foo_t`. Linux C system headers often use the suffix `_t` to denote a `typedef` alias.

### Enumerations <a id="dt_enums_dlang">

Defining symbolic names for constants in a program eases readability and simplifies the process of maintaining the program in the future. One method is to define an *enumeration*, which associates a set of integers with a set of identifiers called enumerators that the compiler recognizes and replaces with the corresponding integer value. An enumeration is defined by using a declaration such as the following:

```
enum colors {
  RED,
  GREEN,
  BLUE
};
```

The first enumerator in the enumeration, `RED`, is assigned the value zero and each subsequent identifier is assigned the next integer value.

You can also specify an explicit integer value for any enumerator by suffixing it with an equal sign and an integer constant, as shown in the following example:

```
enum colors {
  RED = 7,
  GREEN = 9,
  BLUE
};
```

The enumerator `BLUE` is assigned the value `10` by the compiler because it has no value specified and the previous enumerator is set to `9`. When an enumeration is defined, the enumerators can be used anywhere in a D program that an integer constant is used. In addition, the enumeration `enum colors` is also defined as a type that's equivalent to an `int`. The D compiler permits a variable of `enum` type to be used anywhere an `int` can be used and permits any integer value to be assigned to a variable of `enum` type. You can also omit the `enum` name in the declaration, if the type name isn't needed.

Enumerators are visible in all the following clauses and declarations in a program. Therefore, you can't define the same enumerator identifier in more than one enumeration. However, you can define more than one enumerator with the same value in either the same or different enumerations. You can also assign integers that have no corresponding enumerator to a variable of the enumeration type.

The D enumeration syntax is the same as the corresponding syntax in ANSI C. D also provides access to enumerations that are defined in the OS kernel and its loadable modules. Note that these enumerators aren't globally visible in a D program. Kernel enumerators are only visible if you specify one as an argument in a comparison with an object of the corresponding enumeration type. This feature protects D programs against inadvertent identifier name conflicts, with the large collection of enumerations that are defined in the OS kernel.



### Inlines <a id="dt_inlines_dlang">

D named constants can also be defined by using `inline` directives, which provide a more general means of creating identifiers that are replaced by predefined values or expressions during compilation. Inline directives are a more powerful form of lexical replacement than the `#define` directive provided by the C preprocessor because the replacement is assigned an actual type and is performed by using the compiled syntax tree and not a set of lexical tokens. An `inline` directive is specified by using a declaration of the following form:

```
inline *type* *name* = *expression*;
```

where *type* is a type declaration of an existing type, *name* is any valid D identifier that isn't previously defined as an inline or global variable, and *expression* is any valid D expression. After the inline directive is processed, the D compiler substitutes the compiled form of *expression* for each subsequent instance of *name* in the program source.

For example, the following D program would trace the string "`hello`" and integer value `123`:

```
inline string hello = "hello";
inline int number = 100 + 23;

BEGIN
{
  trace(hello);
  trace(number);
}
```

An inline name can be used anywhere a global variable of the corresponding type is used. If the inline expression can be evaluated to an integer or string constant at compile time, then the inline name can also be used in contexts that require constant expressions, such as scalar array dimensions.

The inline expression is validated for syntax errors as part of evaluating the directive. The expression result type must be compatible with the type that's defined by the `inline`, according to the same rules used for the D assignment operator \(`=`\). An inline expression can't reference the `inline` identifier itself: recursive definitions aren't permitted.

The DTrace software packages install several D source files in the system directory `/usr/lib64/dtrace/*installed-version*`, which contain inline directives that you can use in D programs.

For example, the `signal.d` library includes directives of the following form:

```
inline int SIGHUP = 1;
inline int SIGINT = 2;
inline int SIGQUIT = 3;
...
```

These inline definitions provide you with access to the current set of Linux signal names, as described in the `sigaction(2)` manual page. Similarly, the `errno.d` library contains inline directives for the C `errno` constants that are described in the `errno(3)` manual page.

By default, the D compiler includes all the provided D library files automatically so that you can use these definitions in any D program.

### Type Namespaces <a id="dt_typens_dlang">

In traditional languages such as ANSI C, type visibility is determined by whether a type is nested inside a function or other declaration. Types declared at the outer scope of a C program are associated with a single global namespace and are visible throughout the entire program. Types that are defined in C header files are typically included in this outer scope. Unlike these languages, D provides access to types from several outer scopes.

D is a language that provides dynamic observability across different layers of a software stack, including the OS kernel, an associated set of loadable kernel modules, and user processes that are running on the system. A single D program can instantiate probes to gather data from several kernel modules or other software entities that are compiled into independent binary objects. Therefore, more than one data type of the same name, sometimes with different definitions, might be present in the universe of types that are available to DTrace and the D compiler. To manage this situation, the D compiler associates each type with a namespace, which is identified by the containing program object. Types from a particular kernel level object, such as the main kernel or a kernel module, can be accessed by specifying the object name and the back quote \(```\) scoping operator in any type name.

For a kernel module named `foo` that contains the following C type declaration:

```
typedef struct bar {
  int x;
} bar_t;
```

The types `struct bar` and `bar_t` could be accessed from D using the following type names:

```
struct foo`bar
foo`bar_t
```

For example, the kernel includes a `task_struct` that's described in `include/linux/sched.h`. The definition of this struct depends on kernel configuration at build. You can find out information about the struct, such as its size, by referencing it as follows:

```
sizeof(struct vmlinux`task_struct)
```

The back quote operator can be used in any context where a type name is appropriate, including when specifying the type for D variable declarations or cast expressions in D probe clauses.

The D compiler also provides two special, built-in type namespaces that use the names C and D. The C type namespace is initially populated with the standard ANSI C intrinsic types, such as `int`. In addition, type definitions that are acquired by using the C preprocessor \(`cpp`\), by running the `dtrace -C` command, are processed by, and added to the C scope. So, you can include C header files containing type declarations that are already visible in another type namespace without causing a compilation error.

The D type namespace is initially populated with the D type intrinsics, such as `int` and `string`, and the built-in D type aliases, such as `uint64_t`. Any new type declarations that appear in the D program source are automatically added to the D type namespace. If you create a complex type such as a `struct` in a D program consisting of member types from other namespaces, the member types are copied into the D namespace by the declaration.

When the D compiler encounters a type declaration that doesn't specify an explicit namespace using the back quote operator, the compiler searches the set of active type namespaces to find a match by using the specified type name. The C namespace is always searched first, followed by the D namespace. If the type name isn't found in either the C or D namespace, the type namespaces of the active kernel modules are searched in load address order, which doesn't guarantee any ordering properties among the loadable modules. To avoid type name conflicts with other kernel modules, use the scoping operator when accessing types that are defined in loadable kernel modules.

The D compiler uses the compressed ANSI C debugging information that's provided with the core Linux kernel modules to access the types that are associated with the OS source code, without the need to access the corresponding C include files. Note that this symbolic debugging information might not be available for all kernel modules on the system. The D compiler reports an error if you try to access a type within the namespace of a module that lacks the compressed C debugging information that's intended for use with DTrace.
