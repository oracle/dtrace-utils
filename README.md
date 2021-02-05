# Linux DTrace Utils

This is the userspace component of the official Linux port of the DTrace tracing tool.  

Oracle Linux UEK kernel RPMs include DTrace kernel modules for  building and running of the DTrace utilities. 
We also provide pre-built DTrace userspace (utilities) packages for Oracle Linux 6 and Oracle 7. 
The latest packages can be obtained from the following [Oracle Linux yum server](https://yum.oracle.com) repositories: 
- Oracle Linux 7 (x86_64) UEK Release 5 (kernel version 4.14): https://yum.oracle.com/repo/OracleLinux/OL7/UEKR5/x86_64
- Oracle Linux 7 (aarch64) Latest (kernel version 4.14): https://yum.oracle.com/repo/OracleLinux/OL7/latest/aarch64
- Oracle Linux 7 (x86_64) UEK Release 4 (kernel version 4.1.12): https://yum.oracle.com/repo/OracleLinux/OL7/UEKR4/x86_64
- Oracle Linux 6 (x86_64) UEK Release 4 (kernel version 4.1.12): https://yum.oracle.com/repo/OracleLinux/OL6/UEKR4/x86_64

Source code for the UEK kernel is available on github in the [linux-uek repo](https://github.com/oracle/linux-uek).

## Table of Contents

* [1. License](#1-license)
* [2. How to Build dtrace-utils](#2-how-to-build-dtrace-utils)
  * [2.1. Prerequisites](#21-prerequisites)
     * [2.1.1. Step 1](#211-step-1-install-the-compact-type-format-ctf-libraries)
     * [2.1.2. Step 2](#212-step-2-build-a-dtrace-enabled-kernel)
     * [2.1.3. Step 3](#213-step-3-get-the-other-necessary-packages)
  * [2.2. Build dtrace-utils](#22-build-dtrace-utils)
     * [2.2.1. Supporting multiple kernels](#221-supporting-multiple-kernels)
* [3. Packaging Information](#3-packaging-information)
* [4. Testing](#4-testing)
* [5. How to Run DTrace](#5-how-to-run-dtrace)
* [6. Questions](#6-questions)
* [7. Pull Requests and Support](#7-pull-requests-and-support)



## 1. License

DTrace is licensed under the UPL 1.0 (Universal Permissive License).
A copy is included in this repository as the LICENSE file.

## 2. How to Build dtrace-utils

### 2.1. Prerequisites

Please read this section carefully before moving over to the build documentation
to ensure your environment is properly configured.

Prebuilt packages can be obtained from the yum repositories listed above.
We will focus here on the repositories with UEK Release 5 (4.14 based kernel).
The latest yum configuration is available on [Oracle Linux yum server](https://yum.oracle.com).
To enable the **ol7_UEK5** repository ensure that the `enabled=1` is in the `yum`
repository configuration.

#### 2.1.1. Step 1: Install the Compact Type Format (CTF) libraries

CTF type support requires installation of supporting libraries and development
headers. CTF library development is handled as a separate project.
For more information about how to build and install the libraries from source code,
please visit [libdtrace-ctf](https://github.com/oracle/libdtrace-ctf).

Use the following command to install all required packages on Oracle Linux.
```
yum install libdtrace-ctf-devel
```

#### 2.1.2. Step 2: Build a DTrace Enabled Kernel

A Linux kernel with the latest DTrace patches and the DTrace option enabled in the
config file is another major prerequisite.  It is preferred (but not required) to
reboot into your new kernel before trying to build the DTrace utilities.

Oracle Linux's [UEK](https://github.com/oracle/linux-uek) provides a simple alternative
if you don't want to patch, configure, and build your own kernel.
It is possible to install all required dependencies via `yum`.

Use the following command to install all required packages on Oracle Linux.
```
yum install kernel-uek kernel-uek-devel
```

Building of a patched kernel from sources is also straightforward.
Please consult `Documentation/process/changes.rst` in the kernel source tree
to ensure all required dependencies are installed first.
The following steps should build and install the kernel.

```
# Start with your preferred .config options. DTrace should auto-enable itself.
# Features depend on the current state of architectural support.
make olddefconfig
make

# This step will produce vmlinux.ctfa, which holds all CTF data for the kernel
# and its modules.
make ctf

# Install with root privileges.
sudo make modules_install
sudo make install
```

#### 2.1.3. Step 3: Get the Other Necessary Packages

A few remaining packages are required either for building or at runtime.
They should be part of most Linux distributions today
and can be installed via the package management of your distro.

Runtime:

- elfutils
- zlib
- cpp
- libpcap

Build:

- glibc-static
- glibc32 (x86 only)
- elfutils-libelf-devel
- bison
- flex
- glibc-headers
- glibc-devel (x86-32) (x86 only)
- zlib-devel
- libpcap-devel

Package names may vary across various Linux distributions.
For Oracle Linux / UEK, please check [dtrace-utils.spec](dtrace-utils.spec).

### 2.2. Build dtrace-utils

The simplest way of building the utils is booting into your DTrace enabled kernel
and issuing the following commands:

```
cd dtrace-utils
make
sudo make install
```

In this scenario the build system expects that kernel sources can be found at
`/lib/modules/$(uname -r)/build`.  It is also expected that the kernel used for
userspace build is compatible with the utils (the exported userspace headers in
the `include/uapi/linux/dtrace` are compatible with the utils version being built).

#### 2.2.1. Supporting multiple kernels

From time to time you may need to build the utils against a different set of
DTrace's uapi headers, e.g. when a change is done to the headers themselves
during development.  Such a change makes the systemwide installed headers out of date.

One solution is to create a symlink in `/usr/include/linux/dtrace` that points
to the correct set of headers.  An alternative that avoids system modification is
to use the **HDRPREFIX** variable.  The variable points the build system to an
alternate location where DTrace uapi headers can be found.

```
cd dtrace-utils
cp -r <my_kernel_tree>/include/uapi/linux/dtrace <tmp_location>/linux
make HDRPREFIX=<tmp_location>
sudo make install
```

**NOTE**: It is **NOT possible** to point **HDRPREFIX** directly to another kernel tree.
It may appear to work but the build will pull more than just DTrace headers from the
alternate kernel tree, which usually results in failed compilation with lot of
strange errors complaining about incompatible types.

Building with a straight `make` or `make HDRPREFIX=` produces
a utils package that will run only on the currently running kernel and on kernels 
similar to it: other kernels will not have translators available, so DTrace will not work.
To enable support for (possibly many) other kernels you have to do two extra steps:

  1. Install kernel-devel packages for every other kernel that will be used
     during build process, or otherwise put their headers in some consistent
     place.
  2. Tell the build system which kernels you want to build translators for,
     and where their development headers are.  By defaul the build kernel
     is used. (Only the development headers are needed at build time.)

Step 2 is done via several interlocking makefile variables, which together point at
a group of development headers sitting side-by-side in some directory
(you can use symlinks to put the actual headers elsewhere, as the kernel's
own `modules_install` target does):

| Variable        | Default value    | Purpose                            |
|:----------------|:-----------------|:-----------------------------------|
| **KERNELDIRPREFIX** | `/lib/modules` | Directory to find the kernel trees; should be an absolute path         |
| **KERNELS**     | `uname -r`       | Space-separated list of kernel version numbers e.g. 4.1 or 4.14.28-1.el7uek.x86_64, as seen in directory names under the **KERNELDIRPREFIX** |
| **KERNELDIRSUFFIX** | `/build` or `/source` depending on value of **KERNELODIR** | Suffix needed to find the include directory under each **KERNELS** dir: for RPM kernel devel packages, leave empty  |
| **KERNELARCH**  | `uname -m`       | The arch to build against (causes header searches in `arch/**KERNELARCH**/include`) |
| **KERNELODIR**  | Unset            | If you are building directly against a kernel source tree you built with `O=`, the directory name you passed to `O=` |

The defaults suffice to find kernels you built by hand and installed via
'make install', as long as you built each one in a separate source tree so
that all their headers are usable at the same time.

The build system puts these variables together like this to make paths to
find kernel header files in, with **KERNELODIR** and **KERNELARCH** applying to
some of the paths it builds:

```
$KERNELDIRPREFIX/element from $KERNELS/$KERNELDIRSUFFIX/
```

This would build against a group of kernels you installed by hand with
`make modules_install`:
```
cd dtrace-utils
make KERNELS="4.1.12-140.el7.x86_64 4.14.14-29.el7.x86_64"
sudo make install
```

This would build against a group of kernel `devel` packages installed in the OL-standard
kernel development header directory:
```
cd dtrace-utils
make KERNELDIRPREFIX=/usr/src/kernels KERNELDIRSUFFIX= KERNELS="4.1.12-140.el7.x86_64 4.14.14-29.el7.x86_64"
sudo make install
```

Note: The machinery around **KERNELS** is orthogonal to **HDRPREFIX**: you can build
translators against completely different development headers than the kernel the
DTrace uapi headers come from.

For the full detailed explanation of the build system internals,
see [README.build-system](README.build-system).

## 3. Packaging Information

Various Linux distributions use different packaging schemes.
At the moment, the DTrace utilities are shipped in three RPM packages:

- **dtrace-utils** - everything required to run DTrace
- **dtrace-utils-devel** - everything to build the utils or a custom DTrace consumer
- **dtrace-utils-testsuite** - the testsuite

At the moment the provided spec file (`dtrace-utils.spec`) is designed for Oracle
Linux.  It should also work for generic distributions.

After setting up the rpm build environment, the build itself is simple:

```
cd dtrace-utils

# archive sources based on branch/tag
git archive -o ~/rpmbuild/SOURCES/dtrace-utils-1.0.0.tar.bz2 --prefix=dtrace-utils-1.0.0/ 1.0-branch
# or
make dist && mv dtrace-utils-*.tar.bz2 ~/rpmbuild/SOURCES

# do the RPM build
rpmbuild -ba dtrace-utils.spec
```

This will produce RPMs in `~/rpmbuild/RPMS/<arch>/`.  The spec file contains hard
coded kernel version constants that needs updating before the build can succeed.  To
avoid the need to modify the spec file, some overridable macros have been
added.

  - `build_kernel` is the version of the kernel that is used to build the binaries.
    The DTrace kernel headers are selected from this kernel and bundled into the
    dtrace-utils-devel package.
  - `dtrace_kernels` is a list of kernels that are used to generate D translator
    libraries (the list is passed to the **KERNELS** variable explained above).
    The other variables are automatically overridden to get the kernels from
    the kernel devel package directory, `/usr/src/kernels`, as in the second
    example above.
  - `local_build` picks the headers from `/usr/include/linux/dtrace` instead of
    using build kernel.

Examples of use:
```
# Override build kernel from commandline
rpmbuild -ba --define 'build_kernel 1.2.3.el7.x86_64' dtrace-utils.spec

# Use whatever is currently installed on the build host
rpmbuild -ba --define 'local_build 1' dtrace-utils.spec
```

## 4. Testing

At the moment, testing is performed for both **x86-64** and **aarch64** platforms.

A test suite is provided directly in the source tree.
The testsuite is run directly using **make** in conjunction with one of the following targets:

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
The full list is available in the [dtrace-utils.spec](dtrace-utils.spec).
A test suite, including its dependencies, can be installed on Oracle Linux using the following command,
after ensureing that the correct repository is enabled (See [Prerequisites](#prerequisites)):

```
yum install dtrace-utils-testsuite
```

## 5. How to run DTrace

The `dtrace` binary is installed in /usr/**sbin**/dtrace.
Currently, you can only run dtrace with root privileges.
Be careful not to confuse the utility with **SystemTap's dtrace** script,
which is installed in /usr/**bin**/dtrace.


## 6. Questions

For questions, please check the
[dtrace-devel mailing list](https://oss.oracle.com/mailman/listinfo/dtrace-devel).

## 7. Pull Requests and Support

We currently do not accept pull requests via GitHub, please contact us via the mailing list above. 

The source code for dtrace-utils is published here without support. Compiled binaries are provided as part of Oracle Linux, which is [free to download](http://www.oracle.com/technetwork/server-storage/linux/downloads/index.html), distribute and use. 
Support for DTrace is included in Oracle Linux support subscriptions. Individual packages and updates are available on the [Oracle Linux yum server](https://yum.oracle.com). 
