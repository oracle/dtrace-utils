
# Profile Provider <a id="dt_ref_profile_prov">

The `profile` provider includes probes that are associated with an interrupt that fires at some regular, specified time interval.

Such probes aren't associated with any particular point of execution, but rather with the asynchronous interrupt event. You can use these probes to sample some aspect of the system state and then use the samples to infer system behavior. If the sampling rate is high or the sampling time is long, an accurate inference is possible. Using DTrace functions, you can use the `profile` provider to sample many aspects of the system. For example, you could sample the state of the current thread, the state of the CPU, or the current machine instruction.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## profile-*n* Probes <a id="dt_ref_profile-n_prov">

The `profile-*n*` probes fire at a fixed interval, at a high-interrupt level on all active CPUs.

The units of *n* default to a frequency that's expressed as a rate of firing per second,
but the value can also have an optional suffix, as shown in the following table,
which specifies either a time interval or a frequency.
The following table describes valid time suffixes for a `tick-` *n* probe.

| Suffix         | Time Units                                       |
| :---           | :---                                             |
| `nsec` or `ns` | nanoseconds                                      |
| `usec` or `us` | microseconds                                     |
| `msec` or `ms` | milliseconds                                     |
| `sec` or `s`   | seconds                                          |
| `min` or `m`   | minutes                                          |
| `hour` or `h`  | hours                                            |
| `day` or `d`   | days                                             |
| `hz`           | hertz \(frequency expressed as rate per second\) |

## tick-*n* Probes <a id="dt_ref_profile-tick-n_prov">

The `tick-*n*` probes fire at fixed intervals, at a high interrupt level on only one CPU per interval.

Unlike `profile-*n*` probes, which fire on every CPU,
`tick-*n*` probes fire on only one CPU per interval and the CPU on which they fire can change over time.
The units of *n* default to a frequency expressed as a rate of firing per second,
but the value can also have an optional time suffix as shown in the earlier table,
which specifies either a time interval or a frequency.

The `tick-*n*` probes have several uses, such as providing some periodic output or taking a periodic action.

**Note:**

The highest available tick frequency is 5000 Hz \(`tick-5000`\).

## profile Probe Arguments <a id="dt_ref_profargs_prov">

The following table describes the arguments for the `profile` probes.

<table><thead><tr><th>

Probe

</th><th>

`arg0`

</th><th>

`arg1`

</th></tr></thead><tbody><tr><td>

`profile-*n*`

</td><td>

`pc`

</td><td>

`upc`

</td></tr><tr><td>

`tick-*n*`

</td><td>

`pc`

</td><td>

`upc`

</td></tr><tbody></table>
The arguments are as follows:

-   `pc`: kernel program counter

-   `upc`: user-space program counter


## profile Probe Creation <a id="dt_ref_profprobecreate_prov">

Unlike other providers, the `profile` provider creates probes dynamically on an as-needed basis. Thus, the preferred probe might not appear in a listing of all probes, for example, when using the `dtrace -l -P profile` command, but the probe is created when it's explicitly enabled.

A time interval that's too short causes the machine to continuously field time-based interrupts and denies service on the machine. The `profile` provider refuses to create a probe that would result in an interval of less than two hundred microseconds and returns an error.

## prof Stability <a id="dt_ref_profstab_prov">

The `profile` provider uses DTrace's stability mechanism to describe its stabilities. These stability values are listed in the following table.

| Element   | Name Stability | Data Stability | Dependency Class |
| :---      | :---           | :---           | :---             |
| Provider  | Evolving       | Evolving       | Common           |
| Module    | Unstable       | Unstable       | Unknown          |
| Function  | Private        | Private        | Unknown          |
| Name      | Evolving       | Evolving       | Common           |
| Arguments | Evolving       | Evolving       | Common           |
