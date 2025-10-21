
# CPC Provider <a id="dt_ref_cpc_prov">

The CPU performance counter \(`cpc`\) provider makes available probes that are associated with CPU performance counter events.

A probe fires when a specified number of events of a type in a chosen processor mode has occurred. When a probe fires, you can sample aspects of system state and make inferences about system behavior. A reasonable value for the event counter value depends on the event and also on the workload. To keep probe firings from being excessive, start with a high value. Lower the value to improve statistical accuracy.

CPU performance counters are a finite resource and the number of probes that can be enabled depends upon hardware capabilities. An error is returned when the number of `cpc` probes enabled exceed the hardware capability. If hardware resources are unavailable, probes fail until resources become available.

Start with higher event counter values for CPC probes and reduce them through trial-and-error as you work toward a more accurate representation of system activity.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## cpc Probes <a id="dt_ref_cpcprobes_prov">

Probes made available by the `cpc` provider have the following probe description format:

```
cpc:::<event name>-<mode>-<count>
```

The definitions of the components of the probe name are listed in table.

|Component|Meaning|
| :---    | :---  |
|event name|The platform specific or generic event name.|
|mode|The privilege mode in which to count events. Valid modes are *user* for user mode events, *kernel* for kernel mode events and *all* for both user mode and kernel mode events.|
|count|The number of events that must occur on a CPU for a probe to be fired on that CPU. Note that the count is a configurable value. If the count value is too high, then the probe fires less often and the statistics are less reliable. If the count value is too low, the probe fires too often and the system is inundate with tracing activity. When selecting a count value, start with a higher value and then decrease it to get more accurate statistics.|

Note that when you list CPC probes, example count values are provided in the probe listings. The count values are artificially set high as a guideline.

## cpc Probe Arguments <a id="dt_ref_cpcargs_prov">

The following table lists the argument types for the `cpc` probes.

| args | Description |
| :--- | :--- |
|`arg0`|The program counter \(PC\) in the kernel at the time that the probe fired, or 0 if the current process wasn't running in the kernel at the time that the probe fired|
|`arg1`|The PC in the user-level process at the time that the probe fired, or 0 if the current process was running at the kernel at the time that the probe fired|

As the descriptions imply, if `arg0` is non-zero then `arg1` is zero; if `arg0` is zero then `arg1` is non-zero.

## cpc Examples <a id="dt_ref_cpcexamples_prov">

The following example illustrates the use of a probe published by the `cpc` provider.

### cycles-all-50000000

The example performs a count for each process name that triggers the performance counter probe on a count value of 50000000.

```
cpc:::cycles-all-50000000
 {
         @[execname] = count();
 }

```

## cpc Stability <a id="dt_ref_cpcstab_prov">

The `cpc` provider uses DTrace's stability mechanism to describe its stabilities. These values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | Common           |
| Module    | Private        | Private        | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Evolving       | Evolving       | CPU              |
| Arguments | Evolving       | Evolving       | Common           |
