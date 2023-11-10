# Linux DTrace

This is the official Linux port of the DTrace tracing tool.

The source is posted here on github.com in the hope that it increases the
visibility for our work and to make it even easier for people to access the
source.  We also use this repository to work with developers in the Linux
community.

The main development branch is [devel](https://github.com/oracle/dtrace-utils/tree/devel).

We provide prebuilt x86_64 and aarch64 DTrace userspace packages for Oracle
Linux 9 (UEK7 kernel), Oracle Linux 8 (UEK7 or UEK6 kernel), and Oracle Linux 7
(UEK6 kernel).  These packages are based on the Oracle Linux specific release
branch [2.0-branch](https://github.com/oracle/dtrace-utils/tree/2.0-branch).

The latest packages can be obtained from the following
[Oracle Linux yum server](https://yum.oracle.com) repositories: 
- Oracle Linux 9 (x86_64) UEK Release 7: https://yum.oracle.com/repo/OracleLinux/OL9/UEKR7/x86_64/
- Oracle Linux 9 (aarch64) BaseOS Latest: https://yum.oracle.com/repo/OracleLinux/OL9/baseos/latest/aarch64/
- Oracle Linux 8 (x86_64) UEK Release 7: https://yum.oracle.com/repo/OracleLinux/OL8/UEKR7/x86_64/
- Oracle Linux 8 (aarch64) UEK Release 7: https://yum.oracle.com/repo/OracleLinux/OL8/UEKR7/aarch64/
- Oracle Linux 8 (x86_64) UEK Release 6: https://yum.oracle.com/repo/OracleLinux/OL8/UEKR6/x86_64/
- Oracle Linux 8 (aarch64) BaseOS Latest: https://yum.oracle.com/repo/OracleLinux/OL8/baseos/latest/aarch64/
- Oracle Linux 7 (x86_64) UEK Release 6: https://yum.oracle.com/repo/OracleLinux/OL7/UEKR6/x86_64/
- Oracle Linux 7 (aarch64) UEK Release 6: https://yum.oracle.com/repo/OracleLinux/OL7/UEKR6/aarch64/

Source code for the UEK kernel is available on github in the
[linux-uek repo](https://github.com/oracle/linux-uek).

## Table of Contents

* [1. License](#1-license)
* [2. How to Build DTrace](#2-how-to-build-dtrace)
  * [2.1. Prerequisites](#21-prerequisites)
    * [2.1.1. Verify that binutils is recent enough...](#211-verify-that-binutils-is-recent-enough)
    * [2.1.2. ... or install a recent-enough binutils](#212-or-install-a-recent-enough-binutils)
    * [2.1.3. Install other necessary packages](#213-install-other-necessary-packages)
  * [2.2. Build DTrace](#22-build-dtrace)
* [3. Testing](#3-testing)
* [4. How to Run DTrace](#4-how-to-run-dtrace)
* [5. Questions](#5-questions)
* [6. Pull Requests and Support](#6-pull-requests-and-support)
* [7. Building Previous Releases of DTrace](#7-building-previous-releases-of-dtrace)
  * [7.1. Prerequisites](#71-prerequisites)
  * [7.2. Build a kernel with add-on features that DTrace uses](#72-build-a-kernel-with-add-on-features-that-dtrace-uses)
  * [7.3. Build DTrace](#73-build-dtrace)

## 1. License

DTrace is licensed under the UPL 1.0 (Universal Permissive License).
A copy is included in this repository as the LICENSE file.

## 2. How to Build DTrace

The build instructions focus on building DTrace for the upstream Linux kernel.
We will use the 5.14.9 release, but the instructions apply to most recent Linux
kernel versions.

### 2.1. Prerequisites

Please read this section carefully before moving over to the build
documentation to ensure your environment is properly configured.

#### 2.1.1. Verify that binutils is recent enough...

DTrace uses a type introspection system called CTF.  This is supported by
upstream GCC and GNU Binutils.  Make sure you have binutils 2.36 or later
installed.

If your distro provides libctf.so in a binutils development package, you need
to install that too.

#### 2.1.2. or install a recent-enough binutils

If your distro provides binutils 2.36 or later, you should install it.  If not,
you can build your own local copy (which can be configured with a --prefix
specific to itself to avoid disturbing the distro version).

If you cannot install a local copy of binutils, you can alternatively install
[libdtrace-ctf](https://github.com/oracle/libdtrace-ctf).  However, note that
this library is deprecated, and it cannot always read CTF produced by recent
GCC/binutils combinations.

#### 2.1.3. Install other necessary packages

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

### 2.2. Build DTrace

The simplest way of building DTrace is done by issuing the following commands
from the DTrace source tree:

```
make
sudo make install
```

Some distributions install the BPF gcc and binutils under different names.  You
can specify the executables to use using the **BPFC** and **BPFLD** variables.
E.g. on Debian you would use:

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

See the GNUmakefile for more options (building translators against multiple
different kernels at once, building against kernel sources found in
different places, building against a kernel built with O=, etc.)

'make help' might also be of interest.

## 3. Testing

A testsuite is provided in the source tree.  It is run using **make** in
conjunction with one of the following targets:

|          target           | description                                                           |
|:--------------------------|:----------------------------------------------------------------------|
| `check-verbose`           | Tests binaries from build directory. Prints result of every tests.    |
| `check-installed-verbose` | Tests binaries installed in the system. Prints result of every tests. |

Example:
```
sudo make check-verbose
```

Logs from test runs are stored in `test/log/<run_number>/`.
The most recent logs are also available by via the `test/log/current/` symbolic link.

The testsuite itself has more dependencies that need to be installed.
The full list is available in the
[dtrace.spec](https://github.com/oracle/dtrace-utils/blob/devel/dtrace.spec)
file.

## 4. How to run DTrace

The `dtrace` binary is installed in /usr/**sbin**/dtrace.
Currently, you can only run dtrace with root privileges.
Be careful not to confuse the utility with **SystemTap's dtrace** script,
which is installed in /usr/**bin**/dtrace.

## 5. Questions

For questions, please check the
[dtrace-devel mailing list](https://oss.oracle.com/mailman/listinfo/dtrace-devel).

NEW: We have a #linux-dtrace IRC channel on irc.libera.chat, you can
ask questions there as well.

## 6. Pull Requests and Support

We currently do not accept pull requests via GitHub, please contact us
via the mailing list or IRC channel listed above.

The source code for DTrace is published here without support. Compiled binaries are provided as part of Oracle Linux, which is [free to download](http://www.oracle.com/technetwork/server-storage/linux/downloads/index.html), distribute and use.
Support for DTrace is included in Oracle Linux support subscriptions. Individual packages and updates are available on the [Oracle Linux yum server](https://yum.oracle.com). 

## 7. Building Previous Releases of DTrace

For versions of DTrace prior to 2.0.0-1.13.2 a small number of kernel patches
patches are needed, which are
[here](https://github.com/oracle/dtrace-linux-kernel).  Branches named
starting v2/* are suitable.

To build such versions of dtrace follow the steps below.

### 7.1. Prerequisites

See [above](#21-prerequisites).

At this point, you will need an additional step:

### 7.2. Build a kernel with add-on features that DTrace uses

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
# Start with your preferred .config options.  Features DTrace needs should
# enable themselves automatically.
make olddefconfig
make

# This step will produce vmlinux.ctfa, which holds all CTF data for the kernel
# and its modules.  If it doesn't work, you don't have a suitable toolchain
# and/or are missing dependencies needed for the older libdtrace-ctf-based
# tools.
make ctf

# Install with root privileges.
sudo make INSTALL_MOD_STRIP=1 modules_install
sudo make install
```

It is preferred (but not required) to reboot into your new kernel before
trying to build DTrace.  (If you don't reboot into it, you need to specify
some extra options when running make.)

### 7.3. Build DTrace

See [above](#22-build-dtrace).
