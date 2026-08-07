---
name: dtrace-linux
description: Generate and validate runnable DTrace scripts for Linux debugging and performance investigations in an agentic coding framework capable of using SKILLs. Use for incident response, root-cause analysis, latency triage, syscall/process/scheduler/I-O tracing, and stack/profile sampling. Always return complete scripts with safe defaults, stable providers, strict D language constraints, and a verification step before final output.
---

# DTrace for Linux

Generate complete D scripts that compile and run cleanly on Linux systems with DTrace.

## Output Requirements

- Return a full runnable D program every time.
- Include a shebang for script output: `#!/usr/sbin/dtrace -s`.
- Prefer predicates for filtering and aggregations for volume control.
- Include `dtrace:::ERROR` when the script could fault or when robust diagnostics are useful.
- Avoid placeholders, pseudo-code, and partial solutions.

## Safety Rules

- Prefer stable providers: `syscall`, `proc`, `sched`, `profile`, `io`, `pid`, `usdt`, and `dtrace`.
- Avoid dangerous actions unless explicitly necessary.
- Do not recommend `system()` unless there is no viable alternative.
- Treat `copyout*()`, `raise()`, and `system()` as destructive.

## Forbidden Language Constructs

Never use these constructs in D scripts:

- `if`
- `else`
- `for`
- `while`
- `switch`
- `case`
- `default`
- `do`
- `goto`
- `continue`

Use only:

- Predicates: `/expr/`
- Ternary operator: `cond ? a : b`

## Clause Structure

Use this canonical form:

```d
probe-descriptions
/ optional predicate /
{
    statements;
}
```

- Probe description format: `provider:module:function:name`
- Omitted fields are wildcard matches.
- Multiple probes may be comma-separated in one clause.
- Avoid numeric probe IDs.

## Script Skeleton

```d
#!/usr/sbin/dtrace -s

dtrace:::BEGIN
{
    printf("Tracing started...\n");
}

/* tracing clauses */

dtrace:::END
{
    /* printa() for aggregations when needed */
}

dtrace:::ERROR
{
    printf("DTrace error at %s:%s:%s:%s\n", probeprov, probemod, probefunc, probename);
}
```

## Preferred Idioms

- Filter early with predicates:
  - `/execname == "date"/`
  - `/pid == $target/`
- Use per-thread state for timing:
  - `self->ts = timestamp;`
  - `/self->ts/ { @lat = quantize(timestamp - self->ts); self->ts = 0; }`
- Use aggregations instead of unbounded prints:
  - `@counts[key] = count();`
  - `@sum[key] = sum(value);`
  - `@dist = quantize(value);`

## Variables

- Global: `x`, `arr[key]` (shared, not MP-safe by default)
- Thread-local: `self->x` (preferred for correlation)
- Clause-local: `this->x` (temporary per-probe firing)
- Built-in probe/process vars:
  - `pid`, `ppid`, `tid`, `execname`
  - `probeprov`, `probemod`, `probefunc`, `probename`
  - `timestamp`, `vtimestamp`, `errno`
  - `arg0` ... `arg9`, `args[]`
- Macro vars:
  - `$target`, `$pid`, `$uid`, `$1`, `$2`, ...

## Common Functions

- Recording: `trace()`, `printf()`, `printa()`, `exit()`
- Aggregation: `count()`, `sum()`, `avg()`, `min()`, `max()`, `stddev()`, `quantize()`, `lquantize()`, `llquantize()`
- Memory/string: `copyin()`, `copyinstr()`, `strlen()`, `substr()`, `strstr()`
- Stacks: `stack()`, `ustack()`
- Speculation: `speculation()`, `speculate()`, `commit()`, `discard()`

## Provider Quick Reference

- `dtrace`: lifecycle (`BEGIN`, `END`, `ERROR`)
- `syscall`: syscall entry/return
- `proc`: process lifecycle (`exec`, `exit`, ...)
- `sched`: scheduler activity (`on-cpu`, `off-cpu`, ...)
- `profile`: timed sampling (`profile-N`, `tick-N`)
- `io`: I/O start/complete
- `pid`: user-function boundaries in a target process
- `usdt`: user static tracepoints

## Patterns

Count syscalls by executable:

```d
#!/usr/sbin/dtrace -s

syscall:::entry
/execname != ""/
{
    @syscalls[execname] = count();
}

dtrace:::END
{
    printa(@syscalls);
}
```

Time syscall latency:

```d
#!/usr/sbin/dtrace -s

syscall::write:entry
{
    self->ts = timestamp;
}

syscall::write:return
/self->ts/
{
    @lat[execname] = quantize(timestamp - self->ts);
    self->ts = 0;
}
```

Target one process:

```d
#!/usr/sbin/dtrace -s

syscall:::entry
/pid == $target/
{
    @calls[probefunc] = count();
}
```

## Response Style

- Be precise and production-minded.
- Prefer compact scripts with clear predicates.
- Add brief comments only where they prevent ambiguity.
- After presenting a script, include one short run command when useful:
  - `sudo dtrace -s script.d`

## Verification Workflow

Always verify generated scripts before presenting them as final.

1. Write the script to a file, for example `script.d`.
2. Run compile-only verification:
   - `sudo dtrace -e -s script.d`
3. If compile-only is unavailable on the target distro, use a short bounded run:
   - `sudo timeout 3 dtrace -s script.d`
4. Treat verification failures as blocking:
   - Fix the script and re-run verification until it passes.
5. Report verification status with the output:
   - `Verified: yes` plus the command used.
