
# SDT Provider <a id="dt_ref_sdt_prov">

The Statically Defined Tracing \(SDT\) provider \(`sdt`\) is for
tracing markers that have been added manually to the kernel source code.

Historically, DTrace has used SDT instrumentation of the
kernel source code to provide a number of other providers,
such as `io`, `lockstat`, `ip`, `tcp`, and `udp`.
With DTrace on Linux, however, DTrace is a user-space tool,
avoiding modifications of the kernel source code,
implementating those other providers on top of pre-existing probes.

Therefore, the SDT provider now simply exposes kernel *tracepoints*,
introduced by various kernel developers at semantically meaningful points of execution,
making them available as DTrace probes.

Probe arguments are converted from raw arguments provided by the kernel
to a format specified by the tracepoint definition in the kernel source
code and described in
`/sys/kernel/debug/tracing/events/`*subsystem*/*tracepoint*/`format`.

To access the raw arguments,
use the [rawtp](../reference/dtrace_providers_rawtp.md) provider.

For more information on kernel tracepoints, see
<https://github.com/torvalds/linux/blob/master/Documentation/trace/tracepoints.rst>.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## sdt Stability <a id="dt_ref_sdtstab_prov">

The `sdt` provider uses DTrace's stability mechanism to describe its stabilities.
These values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | ISA              |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Private        | Private        | ISA              |
| Arguments | Private        | Private        | ISA              |
