
# Lockstat Provider {#dt_ref_lockstat_prov}

The `lockstat` provider provides probes that can be used to study lock usage and contention.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## lockstat Probes {#dt_ref_lockstatprobes_prov}

For all `lockstat` probes, the module name is `vmlinux` and the function name is an empty string.

Locks are classified as:

-   *Spin*: This is spin wait if there's contention.

-   *Adaptive*: This is either spin wait, or else block, if there's contention.

-   *Readers-writer*.


The following probes fire when a lock is acquired or released:

-   `spin-acquire`.

-   `spin-release`.

-   `adaptive-acquire`.

-   `adaptive-release`.

-   `rw-acquire`.

-   `rw-release`.


The following probe fires before a lock is acquired if there was contention for the lock and if the probe is enabled:

-   `spin-spin`.

-   `adaptive-block`, `adaptive-spin`. One probe fires or else the other, depending on which wait is used.

-   `rw-spin`.


Finally, an `adaptive-acquire-error` probe indicates an error acquiring an adaptive lock.

## lockstat Probe Arguments {#dt_ref_lockstatargs_prov}

The following table lists the argument types for the `lockstat` probes. The `argN` are implementation specific. Use `args[]` to access the probe arguments.

<table><thead><tr><th>

Probe

</th><th>

`args[0]`

</th><th>

`args[1]`

</th><th>

`args[2]`

</th></tr></thead><tbody><tr><td>

`adaptive-acquire`

</td><td>

`struct mutex *`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`adaptive-acquire-error`

</td><td>

`struct mutex *`

</td><td>

`int`

</td><td>

—

</td></tr><tr><td>

`adaptive-block`

</td><td>

`struct mutex *`

</td><td>

`uint64_t`

</td><td>

—

</td></tr><tr><td>

`adaptive-release`

</td><td>

`struct mutex *`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`adaptive-spin`

</td><td>

`struct mutex *`

</td><td>

`uint64_t`

</td><td>

—

</td></tr><tr><td>

`rw-acquire`

</td><td>

`struct rwlock *`

</td><td>

`int`

</td><td>

—

</td></tr><tr><td>

`rw-release`

</td><td>

`struct rwlock *`

</td><td>

`int`

</td><td>

—

</td></tr><tr><td>

`rw-spin`

</td><td>

`struct rwlock *`

</td><td>

`uint64_t`

</td><td>

`int`

</td></tr><tr><td>

`spin-acquire`

</td><td>

`spinlock_t *`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`spin-release`

</td><td>

`spinlock_t *`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`spin-spin`

</td><td>

`spinlock_t *`

</td><td>

`uint64_t`

</td><td>

—

</td></tr><tbody></table>
**Note:**

`args[0]` has a pointer to the lock in question. The probes that fire in case of contention report a `uint64_t` `args[1]`, which is the wait time in nanoseconds. The `rw` probes also report an `int` that's either `RW_READER` or `RW_WRITER`. Finally, `adaptive-acquire-error` reports an `int` with a non zero error.

## lockstat Examples {#dt_ref_lockstatexamples_prov}

The following examples illustrate the use of the probes that are published by the `lockstat` provider.

### adaptive-acquire and spin-acquire

Type the following D source code and save it in a file named `whatlock.d`:

```
lockstat:::spin-acquire,
lockstat:::adaptive-acquire
/pid == $target/
{
  @locks[probename] = count();
}
```

Run the program on the `date` command using `sudo dtrace -qs whatlock.d -c date`. The D output looks similar to:

```
 adaptive-acquire                                                  6
 spin-acquire                                                    134
```

It might be surprising that so many locks are acquired with the `date` command. The large number of locks is a natural artifact of the fine-grained locking required of a scalable system such as the Linux kernel.

## lockstat Stability {#dt_ref_lockstatstab_prov}

The `lockstat` provider uses DTrace's stability mechanism to describe its stabilities. These values are listed in the following table.

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

Evolving

</td><td>

Evolving

</td><td>

ISA

</td></tr><tr><td>

Arguments

</td><td>

Evolving

</td><td>

Evolving

</td><td>

ISA

</td></tr><tbody></table>
