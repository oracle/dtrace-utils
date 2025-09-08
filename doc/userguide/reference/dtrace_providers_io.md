
# IO Provider {#dt_ref_io_prov}

The `io` provider makes available probes that relate to data input and output.

For example, you can use the `io` provider to understand I/O by device, I/O type, I/O size, process, or application name.

**Parent topic:**[DTrace Provider Reference](../reference/dtrace_providers.md)

## io Probes {#dt_ref_ioprobes_prov}

The following table describes the probes for the `io` provider. For all `io` probes, the module is `vmlinux` and the function is an empty string.

<table><thead><tr><th>

Probe

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`start`

</td><td>

Fires when an I/O request is about to be made either to a peripheral device or to an NFS server.

</td></tr><tr><td>

`done`

</td><td>

Fires after an I/O request has been fulfilled. The done probe fires after the I/O completes, but before completion processing has been performed on the buffer. `B_DONE` isn't set in `b_flags` at the time the done probe fires.

</td></tr><tr><td>

`wait-start`

</td><td>

Fires immediately before a thread begins to wait pending completion of an I/O request. Some time after the `wait-start` probe fires, the `wait-done` probe fires in the same thread.

</td></tr><tr><td>

`wait-done`

</td><td>

Fires when a thread finishes waiting for the completion of an I/O request. The `wait-done` probe fires only after the `wait-start` probe has fired in the same thread.

</td></tr><tbody></table>
The `io` probes fire for all I/O requests to peripheral devices, and for all file read and file write requests to an NFS server. Requests for metadata from an NFS server, for example, don't trigger `io` probes because of a `readdir()` request.

## io Probe Arguments {#dt_ref_ioargs_prov}

The following table describes the arguments for the `io` probes. The `argN` are implementation specific. Use `args[]` to access the probe arguments.

<table><thead><tr><th>

Probe

</th><th>

`args[0]`

</th><th>

`args[1]`

</th><th>

`args[2]`

</th></tr></thead><tbody><tr><td>

`start`

</td><td>

`bufinfo_t *`

</td><td>

`devinfo_t *`

</td><td>

`fileinfo_t *`

</td></tr><tr><td>

`done`

</td><td>

`bufinfo_t *`

</td><td>

`devinfo_t *`

</td><td>

`fileinfo_t *`

</td></tr><tr><td>

`wait-start`

</td><td>

`bufinfo_t *`

</td><td>

`devinfo_t *`

</td><td>

`fileinfo_t *`

</td></tr><tr><td>

`wait-done`

</td><td>

`bufinfo_t *`

</td><td>

`devinfo_t *`

</td><td>

`fileinfo_t *`

</td></tr><tbody></table>
**Note:**

DTrace doesn't provide the option to use `fileinfo_t` with `io` probes. In Linux, no information is accessible at the level where the `io` probes fire about the file where an I/O request originated.

### bufinfo\_t {#dt_ref_iobuf_prov}

The `bufinfo_t` structure is the abstraction that describes an I/O request. The buffer that corresponds to an I/O request is pointed to by `args[0]` in the `start`, `done`, `wait-start`, and `wait-done` probes. Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/io.d`. The definition of `bufinfo_t` is as follows:

```nocopybutton
typedef struct bufinfo {
  int b_flags;         /* flags */
  size_t b_bcount;     /* number of bytes */
  caddr_t b_addr;      /* buffer address */
  uint64_t b_lblkno;   /* logical block # on device */
  uint64_t b_blkno;    /* expanded block # on device */
  size_t b_resid;      /* not supported */
  size_t b_bufsize;    /* size of allocated buffer */
  caddr_t b_iodone;    /* I/O completion routine */
  int b_error;         /* not supported */
  dev_t b_edev;        /* extended device */
} bufinfo_t;
```

**Note:**

DTrace translates the members of `bufinfo_t` from the `buffer_head` or `bio` for the Linux I/O request structure, depending on the kernel version.

`b_flags` indicates the state of the I/O buffer, and consists of a bitwise-or of different state values. The following table describes the values for the states.

<table><thead><tr><th>

b\_flags

</th><th>

Value
</th><th>

Description

</th></tr></thead><tbody><tr><td>

`B_ASYNC`

</td><td>

`0x000400`

</td><td>

Indicates that the I/O request is asynchronous and isn't waited upon. The `wait-start` and `wait-done` probes don't fire for asynchronous I/O requests.

 Some I/Os directed to be asynchronous might not set `B_ASYNC`. The asynchronous I/O subsystem could implement the asynchronous request by having a separate worker thread perform a synchronous I/O operation.

</td></tr><tr><td>

`B_BUSY`

</td><td>

`0x000001`

</td><td>

 
</td></tr><tr><td>

`B_DONE`

</td><td>

`0x000002`

</td><td>

 
</td></tr><tr><td>

`B_ERROR`

</td><td>

`0x000004`

</td><td>

 
</td></tr><tr><td>

`B_PAGEIO`

</td><td>

`0x000010`

</td><td>

Indicates that the buffer is being used in a paged I/O request.

</td></tr><tr><td>

`B_PHYS`

</td><td>

`0x000020`

</td><td>

Indicates that the buffer is being used for physical \(direct\) I/O to a user data area.

</td></tr><tr><td>

`B_READ`

</td><td>

`0x000040`

</td><td>

Indicates that data is to be read from the peripheral device into main memory.

</td></tr><tr><td>

`B_WRITE`

</td><td>

`0x000100`

</td><td>

Indicates that the data is to be transferred from main memory to the peripheral device.

</td></tr><tbody></table>
`b_bcount`: Is the number of bytes to be transferred as part of the I/O request.

`b_addr`: Is the virtual address of the I/O request, when known.

`b_lblkno`: Identifies which logical block on the device is to be accessed. The mapping from a logical block to a physical block \(such as the cylinder, track, and so on\) is defined by the device.

`b_blkno`: Identifies which block on the device is to be accessed.

`b_bufsize`: Contains the size of the allocated buffer.

`b_iodone`: Identifies a specific routine in the kernel that's called when the I/O is complete.

`b_edev`: Contains the major and minor device numbers of the device accessed. You can use the D subroutines `getmajor` and `getminor` to extract the major and minor device numbers from the `b_edev` field.

### devinfo\_t {#dt_ref_iodev_prov}

The `devinfo_t` structure provides information about a device. The `devinfo_t` structure that corresponds to the destination device of an I/O is pointed to by `args[1]` in the `start`, `done`, `wait-start`, and `wait-done` probes. Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/io.d`. The definition of `devinfo_t` is as follows:

```nocopybutton
typedef struct devinfo {
  int dev_major;           /* major number */
  int dev_minor;           /* minor number */
  int dev_instance;        /* not supported */
  string dev_name;         /* name of device */
  string dev_statname;     /* name of device + instance/minor */
  string dev_pathname;     /* pathname of device */
} devinfo_t;
```

**Note:**

DTrace translates the members of `devinfo_t` from the `buffer_head` for the Linux I/O request structure.

`dev_major`: Is the major number of the device.

`dev_minor`: Is the minor number of the device.

`dev_name`: Is the name of the device driver that manages the device.

`dev_statname`: Is the name of the device as reported by `iostat`. This field is provided so that aberrant `iostat` output can be quickly correlated to actual I/O activity.

`dev_pathname`: Is the full path of the device. The path that's specified by `dev_pathname` includes components expressing the device node, the instance number, and the minor node. However, note that all three of these elements aren't necessarily expressed in the statistics name. For some devices, the statistics name consists of the device name and the instance number. For other devices, the name consists of the device name and the number of the minor node. So, two devices that have the same `dev_statname` migh differ in their `dev_pathname`.

### fileinfo\_t {#dt_ref_iofile_prov}

**Note:**

On Linux, the `fileinfo_t` argument `args[2]` of the `io` probes isn't supported. However, you can use the `fileinfo_t` structure to obtain information about a process's open files by using the built-in variable `fds[]` array.

The `fileinfo_t` structure provides information about a file. The presence of file information is contingent upon the file system providing this information when dispatching I/O requests. Some file systems, especially third-party file systems, might not provide this information. Also, I/O requests might emanate from a file system for which no file information exists. For example, any I/O from or to file system metadata isn't associated with any one file. Finally, some highly optimized file systems might aggregate I/O from disjoint files into a single I/O request. In this case, the file system might provide the file information either for the file that represents most of the I/O or for the file that represents some I/O. Or, the file system might provide no file information at all in this case.

Detailed information about this data structure can be found in `/usr/lib64/dtrace/*version*/io.d`. The definition of `fileinfo_t` is as follows:

```nocopybutton
typedef struct fileinfo {
  string fi_name;           /* basename */
  string fi_dirname;        /* not supported */
  string fi_pathname;       /* not supported */
  loff_t fi_offset;         /* offset within file */
  string fi_fs;             /* file system */
  string fi_mount;          /* not supported */
  int fi_oflags;            /* open() flags for file descriptor */
} fileinfo_t;
```

The `fi_name` field contains the name of the file but doesn't include any directory components. If no file information is associated with an I/O, the `fi_name` field is set to the string `<none>`. In some rare cases, the pathname that's associated with a file might be unknown. In this case, the `fi_name` field is set to the string `<unknown>`.

The `fi_dirname` field contains only the directory component of the file name. As with `fi_name`, this string can be set to `<none>`, if no file information is present, or `<unknown>` if the pathname that's associated with the file isn't known.

The `fi_pathname` field contains the full pathname to the file. As with `fi_name`, this string can be set to `<none>`, if no file information is present, or `<unknown>` if the pathname that's associated with the file isn't known.

The `fi_offset` field contains the offset within the file , or `-1`, if either file information isn't present or if the offset is otherwise unspecified by the file system.

The `fi_fs` field contains the name of the file system type, or `<none>`, if no information is present.

The `fi_oflags` field contains the flags that were specified when opening the file.

## io Examples {#dt_ref_ioexamples_prov}

The following example script displays information for every I/O as it's issued. Type the following source code and save it in a file named `iosnoop.d`.

```
#pragma D option quiet

BEGIN
{
  printf("%10s %2s\n", "DEVICE", "RW");
}

io:::start
{
  printf("%10s %2s\n", args[1]->dev_statname,
  args[0]->b_flags & B_READ ? "R" : "W");
}
```

The output from this script is similar to the following:

```nocopybutton
    DEVICE RW
     dm-00  R
     dm-00  R
     dm-00  R
     dm-00  R
     dm-00  R
     dm-00  R
...
```

You can make the example script slightly more sophisticated by using an associative array to track the time \(in milliseconds\) spent on each I/O, as shown in the following example:

```
#pragma D option quiet

BEGIN
{
  printf("%10s %2s %7s\n", "DEVICE", "RW", "MS");
}

io:::start
{
  start[args[0]->b_edev, args[0]->b_blkno] = timestamp;
}

io:::done
/start[args[0]->b_edev, args[0]->b_blkno]/
{
  this->elapsed = timestamp - start[args[0]->b_edev, args[0]->b_blkno];
  printf("%10s %2s %3d.%03d\n", args[1]->dev_statname,
  args[0]->b_flags & B_READ ? "R" : "W",
  this->elapsed / 10000000, (this->elapsed / 1000) % 1000);
  start[args[0]->b_edev, args[0]->b_blkno] = 0;
}
```

The changed script adds a MS \(milliseconds\) column to the output.

You can aggregate on device, application, process ID, and bytes transferred, then save it in a file named `whoio.d`, as shown in the following example:

```
#pragma D option quiet

io:::start
{
  @[args[1]->dev_statname, execname, pid] = sum(args[0]->b_bcount);
}

END
{
  printf("%10s %20s %10s %15s\n", "DEVICE", "APP", "PID", "BYTES");
  printa("%10s %20s %10d %15@d\n", @);
}
```

Running this script for a few seconds results in output that's similar to the following:

```nocopybutton
    DEVICE                  APP        PID           BYTES
     dm-00               evince      14759           16384
     dm-00          flush-252:0       1367           45056
     dm-00                 bash      14758          131072
     dm-00       gvfsd-metadata       2787          135168
     dm-00               evince      14758          139264
     dm-00               evince      14338          151552
     dm-00          jbd2/dm-0-8        390          356352
```

If you're copying data from one device to another, you might want to know if one of the devices acts as a limiter on the copy. To answer this question, you need to know the effective throughput of each device, rather than the number of bytes per second that each device is transferring. For exampe, you can find throughput by using the following script and saving it in a file named `copy.d`:

```
#pragma D option quiet

io:::start
{
  start[args[0]->b_edev, args[0]->b_blkno] = timestamp;
}

io:::done
/start[args[0]->b_edev, args[0]->b_blkno]/
{
  /*
   * We want to get an idea of our throughput to this device in KB/sec.
   * What we have, however, is nanoseconds and bytes. That is we want
   * to calculate:
   *
   * bytes / 1024
   * ------------------------
   * nanoseconds / 1000000000
   *
   * But we cannot calculate this using integer arithmetic without losing
   * precision (the denominator, for one, is between 0 and 1 for nearly
   * all I/Os). So we restate the fraction, and cancel:
   *
   * bytes       1000000000      bytes       976562
   * --------- * ------------- = --------- * -------------
   * 1024        nanoseconds     1           nanoseconds
   *
   * This is easy to calculate using integer arithmetic.
   */
  this->elapsed = timestamp - start[args[0]->b_edev, args[0]->b_blkno];
  @[args[1]->dev_statname, args[1]->dev_pathname] =
    quantize((args[0]->b_bcount * 976562) / this->elapsed);
  start[args[0]->b_edev, args[0]->b_blkno] = 0;
}

END
{
  printa(" %s (%s)\n%@d\n", @);
}
```

Running the previous script for several seconds while copying data from a hard disk to a USB drive yields the following output:

```nocopybutton
sdc1 (/dev/sdc1)

           value  ------------- Distribution ------------- count    
              32 |                                         0
              64 |                                         3
             128 |                                         1
             256 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  2257
             512 |                                         1
            1024 |                                         0       

 dm-00 (/dev/dm-00)

           value  ------------- Distribution ------------- count    
             128 |                                         0
             256 |                                         1
             512 |                                         0
            1024 |                                         2
            2048 |                                         0
            4096 |                                         2
            8192 |@@@@@@@@@@@@@@@@@@                       172
           16384 |@@@@@                                    52
           32768 |@@@@@@@@@@@                              108
           65536 |@@@                                      34
          131072 |                                         0     
```

The previous output shows that the USB drive \(`sdc1`\) is clearly the limiting device. The throughput of `sdc1` is between 256K/sec and 512K/sec, while `dm-00` delivered I/O at anywhere from 8 MB/second to over 64 MB/second.

## io Stability {#dt_ref_iostab_prov}

The `io` provider uses DTrace's stability mechanism to describe its stabilities. These values are listed in the following table.

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
