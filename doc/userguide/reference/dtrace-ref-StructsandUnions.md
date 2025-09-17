
# Structs and Unions {#dt_structunion_dlang}

Collections of related variables can be grouped together into composite data objects called *structs* and *unions*. You define these objects in D by creating new type definitions for them. You can use any new types for any D variables, including associative array values. This section explores the syntax and semantics for creating and manipulating these composite types and the D operators that interact with them.

**Parent topic:**[D Program Syntax Reference](../reference/d_program_syntax_reference.md)

## Structs {#dt_structs_dlang}

The D keyword `struct`, short for *structure*, is used to introduce a new type that's composed of a group of other types. The new `struct` type can be used as the type for D variables and arrays, enabling you to define groups of related variables under a single name. D structs are the same as the corresponding construct in C and C++. If you have programmed in the Java programming language, think of a D struct as a class that contains only data members and no methods.

Suppose you want to create a more sophisticated system call tracing program in D that records several things about each `read()` and `write()` system call that's run for an application, for example, the elapsed time, number of calls, and the largest byte count passed as an argument.

You could write a D clause to record these properties in four separate associative arrays, as shown in the following example:

```
int ts[string];       /* declare ts */
int calls[string];    /* declare calls */
int elapsed [string];  /* declare elapsed */
int maxbytes[string]; /* declare maxbytes */ 

syscall::read:entry, syscall::write:entry
/pid == $target/
{
  ts[probefunc] = timestamp;
  calls[probefunc]++;
  maxbytes[probefunc] = arg2 > maxbytes[probefunc] ?
        arg2 : maxbytes[probefunc];
}

syscall::read:return, syscall::write:return
/ts[probefunc] != 0 && pid == $target/
{
  elapsed[probefunc] += timestamp - ts[probefunc];
}

END
{
  printf("       calls max bytes elapsed nsecs\n");
  printf("------ ----- --------- -------------\n");
  printf("  read %5d %9d %d\n",
  calls["read"], maxbytes["read"], elapsed["read"]);
  printf(" write %5d %9d %d\n",
  calls["write"], maxbytes["write"], elapsed["write"]);
}
```

You can make the program easier to read and maintain by using a struct. A struct provides a logical grouping pf data items that belong together. It also saves storage space because all data items can be stored with a single key.

First, declare a new `struct` type at the top of the D program source file:

```
struct callinfo {
  uint64_t ts;       /* timestamp of last syscall entry */
  uint64_t elapsed;  /* total elapsed time in nanoseconds */
  uint64_t calls;    /* number of calls made */
  size_t maxbytes;   /* maximum byte count argument */
};
```

The `struct` keyword is followed by an optional identifier that's used to refer back to the new type, which is now known as `struct callinfo`. The struct members are then within a set of braces `{}` and the entire declaration ends with a semicolon \(`;`\). Each struct member is defined by using the same syntax as a D variable declaration, with the type of the member listed first followed by an identifier naming the member and another semicolon \(`;`\).

The `struct` declaration defines the new type. It doesn't create any variables or allocate any storage in DTrace. When declared, you can use `struct callinfo` as a type throughout the remainder of the D program. Each variable of type `struct callinfo` stores a copy of the four variables that are described by our structure template. The members are arranged in memory in order, according to the member list, with padding space introduced between members, as required for data object alignment purposes.

You can use the member identifier names to access the individual member values using the “`.`” operator by writing an expression of the following form:

```
        `*variable-name*.*member-name*`      
```

The following example is an improved program that uses the new structure type. In a text editor, type the following D program and save it in a file named `rwinfo.d`:

```
struct callinfo {
  uint64_t ts; /* timestamp of last syscall entry */
  uint64_t elapsed; /* total elapsed time in nanoseconds */
  uint64_t calls; /* number of calls made */
  size_t maxbytes; /* maximum byte count argument */
};

struct callinfo i[string]; /* declare i as an associative array */

syscall::read:entry, syscall::write:entry
/pid == $target/
{
  i[probefunc].ts = timestamp;
  i[probefunc].calls++;
  i[probefunc].maxbytes = arg2 > i[probefunc].maxbytes ?
        arg2 : i[probefunc].maxbytes;
}

syscall::read:return, syscall::write:return
/i[probefunc].ts != 0 && pid == $target/
{
  i[probefunc].elapsed += timestamp - i[probefunc].ts;
}

END
{
  printf("       calls max bytes elapsed nsecs\n");
  printf("------ ----- --------- -------------\n");
  printf("  read %5d %9d %d\n",
  i["read"].calls, i["read"].maxbytes, i["read"].elapsed);
  printf(" write %5d %9d %d\n",
  i["write"].calls, i["write"].maxbytes, i["write"].elapsed);
}
```

Run the program to return the results for a command. For example run the `sudo dtrace -q -s rwinfo.d -c /bin/date` command.

```
sudo dtrace -q -s rwinfo.d -c date
```

The `date` program runs and is traced until it exits and fires the `END` probe which prints the results:

```
 ...
       calls max bytes elapsed nsecs 
------ ----- --------- ------------- 
 read     2       4096         10689 
 write    1         29          9817
```

## Pointers to Structs {#dt_ptrstructs_dlang}

Referring to structs by using pointers is common in C and D. You can use the operator `->` to access struct members through a pointer. If `struct s` has a member `m`, and you have a pointer to this struct named `sp`, where `sp` is a variable of type `struct s *`, you can either use the `*` operator to first dereference the `sp` pointer to access the member:

```
struct s *sp;
(*sp).m
```

Or, you can use the `->` operator to achieve the same thing:

```
struct s *sp; 
sp->m
```

DTrace provides several built-in variables that are pointers to structs. For example, the pointer `curpsinfo` refers to `struct` `psinfo` and its content provides a snapshot of information about the state of the process associated with the thread that fired the current probe. The following table lists a few example expressions that use `curpsinfo`, including their types and their meanings.

<table><thead><tr><th>

Example Expression

</th><th>

Type

</th><th>

Meaning

</th></tr></thead><tbody><tr><td>

`curpsinfo->pr_pid`

</td><td>

`pid_t`

</td><td>

Current process ID

</td></tr><tr><td>

`curpsinfo->pr_fname`

</td><td>

`char []`

</td><td>

Executable file name

</td></tr><tr><td>

`curpsinfo->pr_psargs`

</td><td>

`char []`

</td><td>

Initial command line arguments

</td></tr><tbody></table>
The next example uses the `pr_fname` member to identify a process of interest. In an editor, type the following script and save it in a file named `procfs.d`:

```
syscall::write:entry
/ curpsinfo->pr_fname == "date" /
{
  printf("%s run by UID %d\n", curpsinfo->pr_psargs, curpsinfo->pr_uid);
}
```

This clause uses the expression `curpsinfo->pr_fname` to access and match the command name so that the script selects the correct `write()` requests before tracing the arguments. Notice that by using operator `==` with a left-hand argument that's an array of `char` and a right-hand argument that's a string, the D compiler infers that the left-hand argument can be promoted to a string and a string comparison is performed. Type the command `dtrace -q -s procs.d` in one shell and then run several variations of the `date` command in another shell.

```
sudo dtrace -q -s procfs.d 
```

The output that's displayed by DTrace might be similar to the following, indicating that `curpsinfo->pr_psargs` can show how the command is invoked and also any arguments that are included with the command:

```
date  run by UID 500
/bin/date  run by UID 500
date -R  run by UID 500
...
`^C`
```

Complex data structures are used often in C programs, so the ability to describe and reference structs from D also provides a powerful capability for observing the inner workings of the Linux kernel and its system interfaces.

## Unions {#dt_unions_dlang}

Unions are another kind of composite type available in ANSI C and D and are related to structs. A union is a composite type where a set of members of different types are defined and the member objects all occupy the same region of storage. A union is therefore an object of variant type, where only one member is valid at any particular time, depending on how the union has been assigned. Typically, some other variable, or piece of state is used to indicate which union member is currently valid. The size of a union is the size of its largest member. The memory alignment that's used for the union is the maximum alignment required by the union members.

## Member Sizes and Offsets {#dt_memsz_dlang}

You can determine the size in bytes of any D type or expression, including a `struct` or `union`, by using the `sizeof` operator. The `sizeof` operator can be applied either to an expression or to the name of a type surrounded by parentheses, as illustrated in the following two examples:

```
sizeof *expression* 
sizeof (*type-name*)
```

For example, the expression `sizeof (uint64_t)` would return the value `8`, and the expression `sizeof (callinfo.ts)` would also return `8`, if inserted into the source code of the previous example program. The formal return type of the `sizeof` operator is the type alias `size_t`, which is defined as an unsigned integer that's the same size as a pointer in the current data model and is used to represent byte counts. When the `sizeof` operator is applied to an expression, the expression is validated by the D compiler, but the resulting object size is computed at compile time and no code for the expression is generated. You can use `sizeof` anywhere an integer constant is required.

You can use the companion operator `offsetof` to determine the offset in bytes of a struct or union member from the start of the storage that's associated with any object of the `struct` or `union` type. The `offsetof` operator is used in an expression of the following form:

```
offsetof (*type-name*, *member-name*)
```

Here, *type-name* is the name of any `struct` or `union` type or type alias, and *member-name* is the identifier naming a member of that struct or union. Similar to `sizeof`, `offsetof` returns a `size_t` and you can use it anywhere in a D program that an integer constant can be used.

## Bit-Fields {#dt_bitflds_dlang}

D also permits the definition of integer struct and union members of arbitrary numbers of bits, known as *bit-fields*. A bit-field is declared by specifying a signed or unsigned integer base type, a member name, and a suffix indicating the number of bits to be assigned for the field, as shown in the following example:

```
struct s 
{
  int a : 1;
  int b : 3;
  int c : 12;
};
```

The bit-field width is an integer constant that's separated from the member name by a trailing colon. The bit-field width must be positive and must be of a number of bits not larger than the width of the corresponding integer base type. Bit-fields that are larger than 64 bits can't be declared in D. D bit-fields provide compatibility with and access to the corresponding ANSI C capability. Bit-fields are typically used in situations when memory storage is at a premium or when a struct layout must match a hardware register layout.

A bit-field is a compiler construct that automates the layout of an integer and a set of masks to extract the member values. The same result can be achieved by defining the masks yourself and using the `&` operator. The C and D compilers try to pack bits as efficiently as possible, but they're free to do so in any order or fashion. Therefore, bit-fields aren't guaranteed to produce identical bit layouts across differing compilers or architectures. If you require stable bit layout, construct the bit masks yourself and extract the values by using the `&` operator.

A bit-field member is accessed by specifying its name with the “`.`” or `->` operators, similar to any other struct or union member. The bit-field is automatically promoted to the next largest integer type for use in any expressions. Because bit-field storage can't be aligned on a byte boundary or be a round number of bytes in size, you can't apply the `sizeof` or `offsetof` operators to a bit-field member. The D compiler also prohibits you from taking the address of a bit-field member by using the `&` operator.

