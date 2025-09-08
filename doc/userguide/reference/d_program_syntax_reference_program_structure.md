
# Program Structure {#dt_prog_struct}

A D program consists of a set of clauses that describe the probes to enable, an optional predicate that controls when to run, and one or more statements that often describe some functionality to implement when the probe fires.

D programs can also contain declarations of variables and definitions of new types. A probe clause declaration uses the following structure:

```
probe descriptions 
/ predicate / 
{
  statements
}
```

-   **Probe Descriptions**

    Probe descriptions ideally express the full description for a probe and take the form:

    ```
    provider:module:function:name
    ```

    The field descriptors are defined as follows:

    -   **provider**

        The name of the DTrace provider that the probe belongs to.

    -   **module**

        If the probe corresponds to a specific program location, the name of the kernel module, library, or user-space program in which the probe is found. Some probes might be associated with a module name that isn't tied to a particular source location in cases where they relate to more abstract tracepoints.

    -   **function**

        If the probe corresponds to a specific program location, the name of the program function in which the probe is found.

    -   **name**

        The name that provides some idea of the probe's semantic meaning, such as `BEGIN` or `END`.

    DTrace recognizes a form of shorthand when referencing probes. By convention, if you don't specify all the fields of a probe description, DTrace can match a request to all the probes with matching values in the parts of the name that you do specify. For example, you can reference the probe name `BEGIN` in a script to match *any* probe with the name field `BEGIN`, regardless of the value of the provider, module, and function fields. For example, you might see a probe referenced as:

    ```
    BEGIN
    ```

    If a probe is referenced in a D program and it doesn't use a full probe description, the fields are interpreted based on an order of precedence:

    -   A single component matches the probe name, expressed as:

        ```
        *name*
        ```

    -   Two components match the function and probe name, expressed as:

        ```
        *function*:*name*
        ```

    -   Three components match the module, function, and probe name

        ```
        *module*:*function*:*name*
        ```

    Although probes can also be referenced by their ID, this value can change over time. The number of probes on the system doesn't directly correlate to the ID, because new provider modules can be loaded at any time and some providers also offer the ability to create new probes on-the-fly. Avoid using the numerical probe ID to reference a probe.

    Probe descriptions also support a pattern-matching syntax similar to the shell *globbing* pattern matching syntax that's described in the `sh(1)` manual page. For example, you can use the asterisk symbol \(`*`\) to perform a wildcard match, as in the following description:

    ```
    sdt:::tcp*
    ```

    If any fields are blank in the probe description, a wildcard match is performed on that field.

    Unless matching several probes intentionally, specifying the full probe description to avoid unpredictable results is better practice.

<table><thead><tr><th>

Symbol

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`*`

</td><td>

Matches any string, including the null string.

</td></tr><tr><td>

`?`

</td><td>

Matches any single character.

</td></tr><tr><td>

`[]`

</td><td>

Matches any one of the characters inside the square brackets. A pair of characters separated by `-` matches any character between the pair, inclusive. If the first character after the `[` is `!`, any character not within the set is matched.

</td></tr><tr><td>

`\`

</td><td>

Interpret the next character as itself, without any special meaning.

</td></tr><tbody></table>
    To successfully match and enable a probe, the complete probe description must match on every field. A probe description field that isn't a pattern must exactly match the corresponding field of the probe. Note that a description field that's empty matches any probe.

    Several probes can be included in a comma-separated list. By including several probes in the description, the same predicate, and function sequences are applied when each probe is activated.

-   **Predicates**

    Predicates are expressions that appear between a pair of slashes \(`//`\) that are then evaluated at probe firing time to decide whether the associated functions must be processed. Predicates are the primary conditional construct that are used for building more complex control flow in a D program. You can omit the predicate section of the probe clause entirely for any probe so that the functions are always processed when the probe is activated.

    Predicate expressions can use any of the D operators and can include any D data objects such as variables and constants. The predicate expression must evaluate to a value of integer or pointer type so that it can be considered as true or false. As with all D expressions, a zero value is interpreted as false and any non-zero value is interpreted as true.

-   **Statements**

    Statements are described by a list of expressions or functions that are separated by semicolons \(`;`\) and within braces \(`{}`\). An empty set of braces with no statements included causes the default action to be processed. The [Default Action](dtrace-ref-DefaultAction.md) reports the probe activation.


A program can consist of several probe-clause declarations. Clauses run in program order.

A program can be stored on the file system and can be run by the DTrace utility. You can transform a program into an executable script by prepending the file with an interpreter directive that calls the `dtrace` command along with any required options, as a single argument, to run the program. See the `sh(1)` manual page for more information on adding the interpreter line to the beginning of a script. The interpreter directive might look as follows:

```
#!/usr/sbin/dtrace -qs
```

A script can also include D pragma directives to set runtime and compiler options. See [DTrace Runtime and Compile-time Options Reference](dtrace_runtime_options.md) for more information on including this information in a script.

**Parent topic:**[D Program Syntax Reference](../reference/d_program_syntax_reference.md)

