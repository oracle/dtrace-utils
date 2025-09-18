
# Sched Provider {#dt_ref_sched_prov}

The `sched` provider makes available probes related to CPU scheduling.

Because CPUs are the one resource that all threads must consume, the `sched` provider is useful for understanding systemic behavior.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## sched Probes {#dt_ref_schedprob_prov}

The probes for the `sched` provider are listed in the following table. For all `sched` probes, the module is `vmlinux` and the function is an empty string.

<table><thead><tr><th>

Probe

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`dequeue`

</td><td>

Fires immediately before a runnable thread is dequeued from a run queue. The run queue's associated CPU is described by `args[2]`. If there is no associated CPU, the `cpu_id` member of the `args[2]` structure is `-1`."

</td></tr><tr><td>

`enqueue`

</td><td>

Fires immediately before a runnable thread is enqueued to a run queue. If the run queue isn't associated with a particular CPU, the `cpu_id` member of the `args[2]` structure is `-1`. The value in `args[3]` is a Boolean, non zero if the thread is enqueued at the front of the run queue, and zero if at the back.

</td></tr><tr><td>

`off-cpu`

</td><td>

Fires when the current CPU is about to end execution of a thread. The `curcpu` variable indicates the current CPU. The `curlwpsinfo` variable indicates the thread that's ending execution, while `args[0]` and `args[1]` refer to the next thread to run on the CPU.

</td></tr><tr><td>

`on-cpu`

</td><td>

Fires when a CPU has begun execution of a thread. The current CPU, thread, and process, are described by `curcpu`, `curlwpsinfo`, and `curpsinfo`, respectively.

</td></tr><tr><td>

`surrender`

</td><td>

Fires when a CPU has been instructed by another CPU to make a scheduling decision, often because a higher-priority thread has become runnable.

</td></tr><tr><td>

`tick`

</td><td>

Fires as a part of clock tick-based accounting. In clock tick-based accounting, CPU accounting is performed by examining which threads and processes are running when a fixed-interval interrupt fires.

</td></tr><tr><td>

`wakeup`

</td><td>

Fires immediately before the current thread wakes a thread sleeping on a synchronization object. Here, `args[0]` and `args[1]` refer to the sleeping thread, as an `lwpsinfo_t *` and `psinfo_t *`, respectively. The type and address of the synchronization object are contained in the `pr_stype` and `pr_wchan` members of the `lwpsinfo_t` of the sleeping thread. The meaning of this address is a private implementation detail, but the address value might be treated as a token unique to the synchronization object.

</td></tr><tbody></table>
## sched Probe Arguments {#dt_ref_schedargs_prov}

Many of these probes refer to a particular thread. For these probes, the thread's `lwpsinfo_t` is pointed to by `args[0]` and the `psinfo_t` of the process containing the thread by `args[1]`. A few probes refer to a particular CPU. Its `cpuinfo_t` is pointed to by `args[2]`. Only `enqueue` has an `args[3]`, and that argument is a Boolean, as described. The `argN` values are implementation specific. Instead, use `args[]` to access the probe arguments.

The following table contains a summary of the `sched` provider probe arguments.

<table><thead><tr><th>

Probe

</th><th>

`args[0]`

</th><th>

`args[1]`

</th><th>

`args[2]`

</th><th>

`args[3]`

</th></tr></thead><tbody><tr><td>

`dequeue`

</td><td>

`lwpsinfo_t *`

</td><td>

`psinfo_t *`

</td><td>

`cpuinfo_t *`

</td><td>

—

</td></tr><tr><td>

`enqueue`

</td><td>

`lwpsinfo_t *`

</td><td>

`psinfo_t *`

</td><td>

`cpuinfo_t *`

</td><td>

`int`

</td></tr><tr><td>

`off-cpu`

</td><td>

`lwpsinfo_t *`

</td><td>

`psinfo_t *`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`on-cpu`

</td><td>

—

</td><td>

—

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`surrender`

</td><td>

`lwpsinfo_t *`

</td><td>

`psinfo_t *`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`tick`

</td><td>

`lwpsinfo_t *`

</td><td>

`psinfo_t *`

</td><td>

—

</td><td>

—

</td></tr><tr><td>

`wakeup`

</td><td>

`lwpsinfo_t *`

</td><td>

`psinfo_t *`

</td><td>

—

</td><td>

—

</td></tr><tbody></table>
### lwpsinfo\_t and psinfo\_t {#dt_ref_lwpsinfo_t_psinfo_t_sched_prov}

The `lwpsinfo_t` and `psinfo_t` structures are described in [Proc Provider](dtrace_providers_proc.md#).

### cpuinfo\_t {#dt_ref_cpuinfo_sched_prov}

The `cpuinfo_t` structure defines a CPU. The `args[2]` arguments for the `enqueue` and `dequeue` probes point to the `cpuinfo_t` for the CPU associated with the run queue, which is sometimes different from the current CPU, whose `cpuinfo_t` is pointed to by the `curcpu` variable.

The definition of the `cpuinfo_t` structure is:

```nocopybutton
typedef struct cpuinfo {
  processorid_t cpu_id;      
  psetid_t cpu_pset;         /* not supported */
  chipid_t cpu_chip;        
  lgrp_id_t cpu_lgrp;        /* not supported */
} cpuinfo_t;
```

## sched Examples {#dt_ref_schedexamples_prov}

The following examples illustrate the use of the probes that are published by the `sched` provider.

### on-cpu and off-cpu

One common question that you might want answered is which CPUs are running threads and for how long? The following example shows how you can use the `on-cpu` and `off-cpu` probes to easily answer this question on a system-wide basis. Type the following D source code and save it in a file named `where.d`:

```
sched:::on-cpu
{
  self->ts = timestamp;
}

sched:::off-cpu
/self->ts/
{
  @[cpu] = quantize(timestamp - self->ts);
  self->ts = 0;
}
```

Run the script. After a few seconds, cancel the script using `Ctrl-C`. The output looks similar to:

```
        0
          value  ------------- Distribution ------------- count
           2048 |                                         0
           4096 |@@                                       37
           8192 |@@@@@@@@@@@@@                            212
          16384 |@                                        30
          32768 |                                         10
          65536 |@                                        17
         131072 |                                         12
         262144 |                                         9
         524288 |                                         6
        1048576 |                                         5
        2097152 |                                         1
        4194304 |                                         3
        8388608 |@@@@                                     75
       16777216 |@@@@@@@@@@@@                             201
       33554432 |                                         6
       67108864 |                                         0

        1
          value  ------------- Distribution ------------- count
           2048 |                                         0
           4096 |@                                        6
           8192 |@@@@                                     23
          16384 |@@@                                      18
          32768 |@@@@                                     22
          65536 |@@@@                                     22
         131072 |@                                        7
         262144 |                                         5
         524288 |                                         2
        1048576 |                                         3
        2097152 |@                                        9
        4194304 |                                         4
        8388608 |@@@                                      18
       16777216 |@@@                                      19
       33554432 |@@@                                      16
       67108864 |@@@@                                     21
      134217728 |@@                                       14
      268435456 |                                         0
```

The previous output shows that on CPU 1 threads tend to run for less than 131072 nanoseconds \(on order of 100 microseconds\) at a stretch, or for 8388608 to 134217728 nanoseconds \(about 10 to 100 milliseconds\). A noticeable gap between the two clusters of data is shown in the histogram. You also might be interested in knowing which CPUs are running a particular process.

You can also use the `on-cpu` and `off-cpu` probes for answering this question. The following script displays which CPUs run a specified application over a period of ten seconds. Save it in a file named `whererun.d.`:

```
#pragma D option quiet
dtrace:::BEGIN
{
  start = timestamp;
}

sched:::on-cpu
/execname == $$1/
{
  self->ts = timestamp;
}

sched:::off-cpu
/self->ts/
{
  @[cpu] = sum(timestamp - self->ts);
  self->ts = 0;
}

profile:::tick-10sec
{
  exit(0);
}

dtrace:::END
{
  printf("CPU distribution over %d seconds:\n\n",
    (timestamp - start) / 1000000000);
  printf("CPU microseconds\n--- ------------\n");
  normalize(@, 1000);
  printa("%3d %@d\n", @);
}
```

Running the previous script on a large mail server and specifying the IMAP daemon \(using `sudo dtrace -qs whererun.d imapd`\) results in output that's similar to the following:

```
CPU distribution of imapd over 10 seconds:

CPU microseconds
--- ------------
 15 10102
 12 16377
 21 25317
 19 25504
 17 35653
 13 41539
 14 46669
 20 57753
 22 70088
 16 115860
 23 127775
 18 160517
```

Linux considers the amount of time that a thread has been sleeping when selecting a CPU on which to run the thread, as a thread that has been sleeping for less time tends not to migrate. Use the `off-cpu` and `on-cpu` probes to observe this behavior. Type the following source code and save it in a file named `howlong.d`:

```
sched:::off-cpu
/curlwpsinfo->pr_state == SSLEEP/
{
 self->cpu = cpu;
 self->ts = timestamp;
}

sched:::on-cpu
/self->ts/
{
 @[self->cpu == cpu ?
   "sleep time, no CPU migration" : "sleep time, CPU migration"] =
   lquantize((timestamp - self->ts) / 1000000, 0, 500, 25);
 self->ts = 0;
 self->cpu = 0;
}
```

Run the script. After around 30 seconds, cancel the script using `Ctrl`+`C`. The output looks similar to:

```
 sleep time, CPU migration
          value  ------------- Distribution ------------- count
            < 0 |                                         0
              0 |@@@@@@@                                  6838
             25 |@@@@@                                    4714
             50 |@@@                                      3108
             75 |@                                        1304
            100 |@                                        1557
            125 |@                                        1425
            150 |                                         894
            175 |@                                        1526
            200 |@@                                       2010
            225 |@@                                       1933
            250 |@@                                       1982
            275 |@@                                       2051
            300 |@@                                       2021
            325 |@                                        1708
            350 |@                                        1113
            375 |                                         502
            400 |                                         220
            425 |                                         106
            450 |                                         54
            475 |                                         40
         >= 500 |@                                        1716

 sleep time, no CPU migration
          value  ------------- Distribution ------------- count
            < 0 |                                         0
              0 |@@@@@@@@@@@@                             58413
             25 |@@@                                      14793
             50 |@@                                       10050
             75 |                                         3858
            100 |@                                        6242
            125 |@                                        6555
            150 |                                         3980
            175 |@                                        5987
            200 |@                                        9024
            225 |@                                        9070
            250 |@@                                       10745
            275 |@@                                       11898
            300 |@@                                       11704
            325 |@@                                       10846
            350 |@                                        6962
            375 |                                         3292
            400 |                                         1713
            425 |                                         585
            450 |                                         201
            475 |                                         96
         >= 500 |                                         3946
```

The previous output reveals more occurrences of non-migration than migration. Also, when sleep times are longer, migrations are more likely. The distributions are different in the under 100 millisecond range, but look similar as the sleep times get longer. This result would seem to indicate that sleep time isn't factored into the scheduling decision when a certain threshold is exceeded.

### enqueue and dequeue

You might want to know on which CPUs processes and threads are waiting to run. You can use the `enqueue` probe along with the `dequeue` probe to answer this question. Type the following source code and save it in a file named `qtime.d`:

```
sched:::enqueue
{
  a[args[0]->pr_lwpid, args[1]->pr_pid, args[2]->cpu_id] =
  timestamp;
}

sched:::dequeue
/a[args[0]->pr_lwpid, args[1]->pr_pid, args[2]->cpu_id]/
{
  @[args[2]->cpu_id] = quantize(timestamp -
    a[args[0]->pr_lwpid, args[1]->pr_pid, args[2]->cpu_id]);
  a[args[0]->pr_lwpid, args[1]->pr_pid, args[2]->cpu_id] = 0;
}
```

Running the previous script for several seconds results in output that's similar to the following:

```
        1
           value  ------------- Distribution ------------- count    
            8192 |                                         0        
           16384 |                                         1        
           32768 |@                                        47       
           65536 |@@@@@@@                                  365      
          131072 |@@@@@@@@@@@@                             572      
          262144 |@@@@@@@@@@@@                             570      
          524288 |@@@@@@@                                  354      
         1048576 |@                                        57       
         2097152 |                                         7        
         4194304 |                                         1        
         8388608 |                                         1        
        16777216 |                                         0        

        0
           value  ------------- Distribution ------------- count    
            8192 |                                         0        
           16384 |                                         6        
           32768 |@                                        49       
           65536 |@@@@@                                    261      
          131072 |@@@@@@@@@@@@@                            753      
          262144 |@@@@@@@@@@@@                             704      
          524288 |@@@@@@@@                                 455      
         1048576 |@                                        74       
         2097152 |                                         9        
         4194304 |                                         2        
         8388608 |                                         0
```

Rather than looking at wait times, you might want to examine the length of the run queue over time. Using the `enqueue` and `dequeue` probes, you can set up an associative array to track the queue length. Type the following source code and save it in a file named `qlen.d`:

```
sched:::enqueue
{
  this->len = qlen[args[2]->cpu_id]++;
  @[args[2]->cpu_id] = lquantize(this->len, 0, 100);
}

sched:::dequeue
/qlen[args[2]->cpu_id]/
{
  qlen[args[2]->cpu_id]--;
}
```

Running the previous script on a largely idle dual-core processor system for about 30 seconds results in output that's similar to the following:

```
        1
           value  ------------- Distribution ------------- count    
             < 0 |                                         0        
               0 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@        8124     
               1 |@@@@@@                                   1558     
               2 |@                                        160      
               3 |                                         51       
               4 |                                         24       
               5 |                                         13       
               6 |                                         11       
               7 |                                         9        
               8 |                                         6        
               9 |                                         0        

        0
           value  ------------- Distribution ------------- count    
             < 0 |                                         0        
               0 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@           8569     
               1 |@@@@@@@@@                                2429     
               2 |@                                        292      
               3 |                                         25       
               4 |                                         8        
               5 |                                         5        
               6 |                                         4        
               7 |                                         4        
               8 |                                         1        
               9 |                                         0
```

The output is what you might expect for an idle system: most the time that a runnable thread is enqueued, the run queues were short \(three or fewer threads in length\). However, as that the system was largely idle, the exceptional data points at the bottom of each table might be unexpected. For example, why were the run queues as long as 8 runnable threads? To explore this question further, you could write a D script that displays the contents of the run queue when the length of the run queue is long. This problem is complicated because D probes can't iterate over data structures, and therefore can't iterate over the entire run queue. Even if D probes could do so, avoid dependencies on the kernel's internal data structures.

For this type of script, you would enable the `enqueue` and `dequeue` probes and then use both speculations and associative arrays. For example, type the following source code and save it in a file named `whoqueue.d`:

```
#pragma D option quiet
#pragma D option nspec=4
#pragma D option specsize=100k

int maxlen;
int spec[int];
sched:::enqueue
{
  this->len = ++qlen[this->cpu = args[2]->cpu_id];
  in[args[0]->pr_addr] = timestamp;
}

sched:::enqueue
/this->len > maxlen && spec[this->cpu]/
{
  /*
   * There is already a speculation for this CPU. We just set a new
   * record, so we’ll discard the old one.
   */
  discard(spec[this->cpu]);
}

sched:::enqueue
/this->len > maxlen/
{
  /*
   * We have a winner. Set the new maximum length and set the timestamp
   * of the longest length.
   */
  maxlen = this->len;
  longtime[this->cpu] = timestamp;
  /*
   * Now start a new speculation, and speculatively trace the length.
   */
  this->spec = spec[this->cpu] = speculation();
  speculate(this->spec);
  printf("Run queue of length %d:\n", this->len);
}

sched:::dequeue
/(this->in = in[args[0]->pr_addr]) &&
  this->in <= longtime[this->cpu = args[2]->cpu_id]/
{
  speculate(spec[this->cpu]);
  printf(" %d/%d (%s)\n",
    args[1]->pr_pid, args[0]->pr_lwpid,
    stringof(args[1]->pr_fname));
}

sched:::dequeue
/qlen[args[2]->cpu_id]/
{
  in[args[0]->pr_addr] = 0;
  this->len = --qlen[args[2]->cpu_id];
}

sched:::dequeue
/this->len == 0 && spec[this->cpu]/
{
  /*
   * We just processed the last thread that was enqueued at the time
   * of longest length; commit the speculation, which by now contains
   * each thread that was enqueued when the queue was longest.
   */
  commit(spec[this->cpu]);
  spec[this->cpu] = 0;
}
```

In this script, whenever a thread is enqueued, it increments the length of the queue and records the timestamp in an associative array keyed by the thread. You can't use a thread-local variable in this case because a thread might be enqueued by another thread. The script then checks to see if the queue length exceeds the maximum, and if so, the script starts a new speculation, and records the timestamp and the new maximum. Then, when a thread is dequeued, the script compares the `enqueue` timestamp to the timestamp of the longest length: if the thread was enqueued before the timestamp of the longest length, the thread was in the queue when the longest length was recorded. In this case, the script speculatively traces the thread's information. When the kernel dequeues the last thread that was enqueued at the timestamp of the longest length, the script commits the speculation data.

Running the previous script on the same system results in output that's similar to the following:

```
Run queue of length 1:
 2850/2850 (java)
Run queue of length 2:
 4034/4034 (kworker/0:1)
 16/16 (sync_supers)
Run queue of length 3:
 10/10 (ksoftirqd/1)
 1710/1710 (hald-addon-inpu)
 25350/25350 (dtrace)
Run queue of length 4:
 2852/2852 (java)
 2850/2850 (java)
 1710/1710 (hald-addon-inpu)
 2099/2099 (Xorg)
Run queue of length 5:
 3149/3149 (notification-da)
 2417/2417 (gnome-settings-)
 2437/2437 (gnome-panel)
 2461/2461 (wnck-applet)
 2432/2432 (metacity)
Run queue of length 9:
 3685/3685 (firefox)
 3149/3149 (notification-da)
 2417/2417 (gnome-settings-)
 2437/2437 (gnome-panel)
 2852/2852 (java)
 2452/2452 (nautilus)
 2461/2461 (wnck-applet)
 2432/2432 (metacity)
 2749/2749 (gnome-terminal)
`^C`
```

### wakeup

The following example shows how you might use the `wakeup` probe to find what's waking a particular process, and when, over a time period. Type the following source code and save it in a file named `gterm.d`:

```
#pragma D option quiet

dtrace:::BEGIN
{
  start = timestamp;
}

sched:::wakeup
/stringof(args[1]->pr_fname) == "gnome-terminal"/
{
  @[execname] = lquantize((timestamp - start) / 1000000000, 0, 10);
}

profile:::tick-10sec
{
  exit(0);
}
```

Running this script results in output similar to:

```
  Xorg                                              
           value  ------------- Distribution ------------- count    
             < 0 |                                         0        
               0 |@@@@@@@@@@@@@@@                          69       
               1 |@@@@@@@@                                 35       
               2 |@@@@@@@@@                                42       
               3 |                                         2        
               4 |                                         0        
               5 |                                         0        
               6 |                                         0        
               7 |@@@@                                     16       
               8 |                                         0        
               9 |@@@                                      15       
           >= 10 |                                         0  
```

This output shows that the X server is waking the `gnome-terminal` process as you interact with the system.

### tick

Linux might use *tick-based CPU accounting*, where a system clock interrupt fires at a fixed interval and attributes CPU utilization to the processes that are running at the time of the tick. The following example shows how you would use the `tick` probe to observe this attribution.

```
sudo dtrace -n sched:::tick'{ @[stringof(args[1]->pr_fname)] = count() }'
```

Enter `Ctrl`+`C`, and output similar to the following is shown:

```
  VBoxService                                                       1
  gpk-update-icon                                                   1
  hald-addon-inpu                                                   1
  jbd2/dm-0-8                                                       1
  automount                                                         2
  gnome-session                                                     2
  hald                                                              2
  gnome-power-man                                                   3
  ksoftirqd/0                                                       3
  kworker/0:2                                                       3
  notification-da                                                   4
  devkit-power-da                                                   6
  nautilus                                                          9
  dbus-daemon                                                      11
  gnome-panel                                                      11
  gnome-settings-                                                  11
  dtrace                                                           19
  khugepaged                                                       22
  metacity                                                         27
  kworker/0:0                                                      41
  swapper                                                          56
  firefox                                                          58
  wnck-applet                                                      61
  gnome-terminal                                                   67
  java                                                             84
  Xorg                                                            227
```

One deficiency of tick-based accounting is that the system clock that performs accounting is often also responsible for dispatching any time-related scheduling activity. If a thread is to perform some amount of work every clock tick \(say, every 10 milliseconds\), the system either over accounts or under accounts for the thread, depending on whether the accounting is done before or after time-related dispatching scheduling activity. If accounting is performed before time-related dispatching, the system under accounts for threads running at a regular interval. If such threads run for less than the clock tick interval, they can effectively hide behind the clock tick.

The following example examines whether a system has any such threads. Type the following source code and save it in a file named `tick.d`:

```
sched:::tick,
sched:::enqueue
{
  @[probename] = lquantize((timestamp / 1000000) % 10, 0, 10);
}
```

The output of the example script is two distributions of the millisecond offset within a ten millisecond interval, one for the `tick` probe and another for `enqueue`:

```
  tick                                              
           value  ------------- Distribution ------------- count    
             < 0 |                                         0        
               0 |@@@@@                                    29       
               1 |@@@@@@@@@@@@@@@@@@@                      106      
               2 |@@@@@                                    27       
               3 |@                                        7        
               4 |@@                                       10       
               5 |@@                                       12       
               6 |@                                        4        
               7 |@                                        8        
               8 |@@                                       9        
               9 |@@@                                      17       
           >= 10 |                                         0        

  enqueue                                           
           value  ------------- Distribution ------------- count    
             < 0 |                                         0        
               0 |@@@@                                     82       
               1 |@@@@                                     86       
               2 |@@@@                                     76       
               3 |@@@                                      65       
               4 |@@@@@                                    101      
               5 |@@@@                                     79       
               6 |@@@@                                     75       
               7 |@@@@                                     76       
               8 |@@@@                                     89       
               9 |@@@@                                     75       
           >= 10 |                                         0 
```

The output histogram named `tick` shows that the clock tick is firing at a 1 millisecond offset. In this example, the output for `enqueue` is evenly spread across the ten millisecond interval and no spike is visible at 1 millisecond, so it seems the threads aren't being scheduled on a time basis.

## sched Stability {#dt_ref_schedstab_prov}

The `sched` provider uses DTrace's stability mechanism to describe its stabilities. These values are listed in the following table.

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
