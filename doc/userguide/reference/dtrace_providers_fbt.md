
# FBT Provider {#dt_ref_fbt_prov}

The `fbt` \(Function Boundary Tracing\) provider includes probes that are associated with the entry to and return from most functions in the Linux kernel. Therefore, there could be tens of thousands of `fbt` probes.

While the FBT implementation is highly specific to the instruction set architecture, FBT has been implemented on both x86 and 64-bit Arm platforms. Some functions in each instruction set are highly optimized by the compiler and can't be instrumented by FBT. Probes for these functions aren't present in DTrace, but you can check what's available by running:

```
sudo dtrace -lP fbt
```

An effective use of FBT probes requires knowledge of the kernel implementation. Therefore, we recommend that you use FBT only when developing kernel software or when other providers aren't sufficient.

Because of the large number of FPB probes that are available, be specific about the modules and functions that you enable probes for. Performance can be impacted when the full range of FBT probes are enabled at the same time.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## fbt Probes {#dt_ref_fbtprobes_prov}

FBT provides an `entry` probe and a `return` probe for most functions in the kernel.

## fbt Probe Arguments {#dt_ref_fbtargs_prov}

The arguments to `entry` probes are the same as the arguments to the corresponding operating system kernel function. These arguments can be accessed as `int64_t` values by using the `arg0`, `arg1`, `arg2`, ... variables.

If the function has a return value, the return value is stored in `arg1` of the `return` probe. If a function doesn't have a return value, `arg1` isn't defined.

## fbt Examples {#dt_ref_fbtexamples_prov}

You can use the `fbt` provider to explore the kernel's implementation. The following example script creates an aggregation on the number of times different functions allocate kernel virtual memory. The results of the aggregation are printed when the script exits. This would help somebody to monitor what functions are memory intensive. Type the following D source code and save it in a file named `getkmemalloc.d`:

```
#pragma D option quiet
fbt::kmem*alloc*:entry 
{ 
  @[caller] = count(); 
} 
dtrace:::END 
{ 
  printa("%40a %@10d\n", @); 
}
```

Running this script results in output similar to the following:

```
              vmlinux`vm_area_alloc+0x1a          1
           vmlinux`__sigqueue_alloc+0x65          1
          vmlinux`__create_xol_area+0x4d          1
          vmlinux`__create_xol_area+0x6f          1
               vmlinux`vmstat_start+0x39          1
           vmlinux`proc_alloc_inode+0x1d          1
         vmlinux`proc_self_get_link+0x5b          1
       vmlinux`security_inode_alloc+0x24          1
             vmlinux`avc_alloc_node+0x1c          1
       vmlinux`ep_ptable_queue_proc+0x3d          2
            vmlinux`kernfs_fop_open+0xbf          2
           vmlinux`kernfs_fop_open+0x2e8          2
            vmlinux`disk_seqf_start+0x25          2
               vmlinux`__alloc_skb+0x16c          6
                  vmlinux`skb_clone+0x4b          6
                  vmlinux`ep_insert+0xbb          8
                 vmlinux`ep_insert+0x34c          8
                  vmlinux`__d_alloc+0x29          9
        vmlinux`kernfs_iop_get_link+0x33          9
                vmlinux`single_open+0x2a         15
              vmlinux`proc_reg_open+0x6e         17
                   vmlinux`seq_open+0x2a         21
               vmlinux`__alloc_file+0x23         29
        vmlinux`security_file_alloc+0x24         29
       vmlinux`getname_flags.part.0+0x2c         40

```

The output shows the internal kernel functions that are making calls to the `kmem*alloc` system calls and can be used to find which kernel functions most often allocate kernel virtual memory on a system.

## fbt Stability {#dt_ref_fbtstab_prov}

The `fbt` provider uses DTrace's stability mechanism to describe its stabilities. These stability values are listed in the following table.

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

Common

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

ISA

</td></tr><tr><td>

Name

</td><td>

Evolving

</td><td>

Evolving

</td><td>

Common

</td></tr><tr><td>

Arguments

</td><td>

Private

</td><td>

Private

</td><td>

ISA

</td></tr><tbody></table>
