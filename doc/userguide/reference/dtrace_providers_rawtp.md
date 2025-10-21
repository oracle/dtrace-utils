
# Rawtp Provider <a id="dt_ref_rawtp_prov">

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

## rawtp Stability <a id="dt_ref_rawtpstab_prov">

The `rawtp` provider uses DTrace's stability mechanism to describe its stabilities. These stability values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | ISA              |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Private        | Private        | ISA              |
| Arguments | Private        | Private        | ISA              |
