
# Rawtp Provider {#dt_ref_rawtp_prov}

The `rawtp` provider gives DTrace users access to the raw tracepoints exposed by the kernel tracing system, including access to the untranslated arguments of the associated tracepoint events.

To see what raw tracepoints are available on a system, use:

```
sudo dtrace -lP rawtp
```

To see the types of the untranslated arguments, use:

```
sudo dtrace -lvP rawtp
```

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## rawtp Stability {#dt_ref_rawtpstab_prov}

The `rawtp` provider uses DTrace's stability mechanism to describe its stabilities. These stability values are listed in the following table.

<table><thead><tr><th>

Element

</th><th>

Name Stability

</th><th>

Data Stability

</th><th>

Dependency Class

</th></tr></thead><tbody><tr><td>

Provider

</td><td>

Evolving

</td><td>

Evolving

</td><td>

ISA

</td></tr><tr><td>

Module

</td><td>

Private

</td><td>

Private

</td><td>

Unknown

</td></tr><tr><td>

Function

</td><td>

Private

</td><td>

Private

</td><td>

Unknown

</td></tr><tr><td>

Name

</td><td>

Private

</td><td>

Private

</td><td>

ISA

</td></tr><tr><td>

Arguments

</td><td>

Private

</td><td>

Private

</td><td>

ISA

</td></tr><tbody></table>
