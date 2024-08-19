 Linux DTrace

For versions of DTrace prior to 2.0.0-1.13.2 a small number of kernel patches
patches are needed, which are
[here](https://github.com/oracle/dtrace-linux-kernel).  Branches named
starting v2/* are suitable.

To build such versions of dtrace follow the steps below.

## 1. Prerequisites

Please read this section carefully before moving over to the build
documentation to ensure your environment is properly configured.

### 1.1. Verify that binutils is recent enough...

DTrace uses a type introspection system called CTF.  This is supported by
upstream GCC and GNU Binutils.  Make sure you have binutils 1.36 or later
installed.

If your distro provides libctf.so in a binutils development package, you need
to install that too.

### 1.2. or install a recent-enough binutils

If your distro provides binutils 1.36 or later, you should install it.  If not,
you can build your own local copy (which can be configured with a --prefix
specific to itself to avoid disturbing the distro version).

### 1.3. Install other necessary packages

A few other packages are required, either for building or at runtime.  They
should be part of most Linux distributions today and can be installed via the
package manager of your distro.  The table below gives package names for Debian
and Oracle Linux.

For building:

| Prerequisite       | Debian                      | Oracle Linux              |
|:-------------------|:----------------------------|:--------------------------|
| glibc headers      | libc6-dev                   | glibc-headers             |
| glibc (static)     | (in libc6-dev)              | glibc-static              |
| glibc (32-bit dev) | libc6-dev-i386              | glibc-devel.i686          |
| bison              | bison                       | bison                     |
| flex               | flex                        | flex                      |
| BPF gcc            | gcc-bpf                     | gcc-bpf-unknown-none      |
| BPF binutils       | binutils-bpf                | binutils-bpf-unknown-none |
| libpcap dev        | libpcap-dev                 | libpcap-devel             |
| wireshark          | wireshark                   | wireshark                 |
| valgrind           | valgrind                    | valgrind-devel            |
| fuse3 or fuse      | libfuse3-dev or libfuse-dev | fuse3-devel or fuse-devel |
| kernel headers     | linux-headers-<kver>-<arch> | kernel-uek-devel-<kver>   |
|                    | linux-headers-<kver>-common |                           |

At runtime:

| Prerequisite       | Debian                 | Oracle Linux              |
|:-------------------|:-----------------------|:--------------------------|
| wireshark          | wireshark              | wireshark                 |
| fuse3 or fuse      | libfuse3-3 or libfuse2 | fuse3-devel or fuse-devel |

## 2. Build a kernel with add-on features that DTrace uses

DTrace 2.0.0-1.13.1 and earlier depend on a few extra kernel features that are
not available in the upstream kernel:

- CTF type information extraction
- /proc/kallmodsyms
- New system call: waitfd()

As noted above, patches that implement these features are available from the
v2/* branches in our Linux kernel repository for features that DTrace uses:
https://github.com/oracle/dtrace-linux-kernel

The customary way to obtain these patches is to clone an appropriate upstream
kernel version (5.8.1 or later) and to merge our
[updated tree](https://github.com/oracle/dtrace-linux-kernel/tree/v2/5.8.1)
into it.  The patch set is quite small, but may need some tweaking if it is
being merged into a newer tree.  We occasionally push a new patched tree to our
github repo, so do look around the repo to see if there is a branch with better
match.

Oracle Linux's [UEK7](https://github.com/oracle/linux-uek/tree/uek7/ga)
provides a simple alternative if you don't want to patch your own kernel.

Building of a patched kernel from sources is also straightforward.
Please consult `Documentation/process/changes.rst` in the kernel source tree
to ensure all required dependencies are installed first.  The following steps
should build and install the kernel.

```
 Start with your preferred .config options.  Features DTrace needs should
 enable themselves automatically.
make olddefconfig
make

 This step will produce vmlinux.ctfa, which holds all CTF data for the kernel
 and its modules.  If it doesn't work, you don't have a toolchain that is
 recent enough.
make ctf

 Install with root privileges.
sudo make INSTALL_MOD_STRIP=1 modules_install
sudo make install
```

It is preferred (but not required) to reboot into your new kernel before
trying to build DTrace.  (If you don't reboot into it, you need to specify
some extra options when running make.)

## 3. Build DTrace

The simplest way of building DTrace is done by issuing the following commands
from the DTrace source tree:

```
make
sudo make install
```

Some distributions install the BPF gcc and binutils under different names.  You
can specify the executables to use using the **BPFC** and **BPFLD** variables.
E.g. on Debian you could use:

```
make BPFC=bpf-gcc BPFLD=bpf-ld
sudo make install
```

In this scenario the build system expects that kernel sources can be found at
`/lib/modules/$(uname -r)/build`.  It is also expected that the kernel used for
userspace build is compatible with the utils (the exported userspace headers in
the `include/uapi/linux/dtrace` are compatible with the utils version being
built).

You can point at a different kernel version with the KERNELS variable, e.g.
```
make KERNELS="5.16.8"
```
as long as the source tree that kernel was built with remains where it was
when that kernel was installed.

See `./configure --help`, `make help`, and the top-level GNUmakefile for a
full list of options (building translators against multiple different
kernels at once, building against kernel sources found in different places,
building against a kernel built with O=, installing in different places,
etc.)

Some of the options (e.g., those specifying paths) may need to be specified
when installing as well as when building.  To avoid this, you can use the
configure script: it bakes variable settings into the makefile so that they
persist across multiple invocations, including `make install`.
