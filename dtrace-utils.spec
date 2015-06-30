# spec file for package dtrace-utils.
#
# Copyright 2011, 2012, 2013, 2014, 2015 Oracle, Inc.  All rights reserved.
#

# Redefine 'build_variant' at build time to create a kernel package named
# something like 'kernel-uek-dtrace'.
%define variant %{?build_variant:%{build_variant}}%{!?build_variant:-uek}

# A list of kernels that you want dtrace to be runnable against. The
# resulting dtrace will work on any kernel with the same major.minor
# version.
%{lua: dtrace_kernels = {"3.8.13-85"}}

BuildRequires: rpm
Name:         dtrace-utils
License:      Oracle Corporation
Group:        Development/Tools
Provides:     dtrace-utils
Requires:     cpp elfutils-libelf zlib libdtrace-ctf dtrace-modules-shared-headers yum
BuildRequires: glibc-static glibc-devel(%{__isa_name}-32) libgcc(%{__isa_name}-32) elfutils-libelf-devel libdtrace-ctf-devel glibc-headers bison flex zlib-devel dtrace-modules-shared-headers
Summary:      DTrace user interface.
Version:      0.4.6
Release:      3%{?dist}
Source:       dtrace-utils-%{version}.tar.bz2
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
ExclusiveArch:    x86_64

# Substitute in kernel-version-specific requirements.

%{lua:
  local srcdirexp = ""
  for i, k in ipairs(dtrace_kernels)  do
      print(rpm.expand("BuildRequires: kernel%{variant}-devel = " .. k .. "%{?dist}uek"))
      srcdirexp = srcdirexp .. " " .. k .. "*"
  end
  rpm.define("srcdirexp " .. srcdirexp)
}

%description
DTrace user interface and dtrace(1) command.

Maintainers:
-----------
Nick Alcock <nick.alcock@oracle.com>
Kris van Hees <kris.van.hees@oracle.com>

%package devel
Summary:      DTrace development headers.
Requires:     libdtrace-ctf-devel > 0.4.0 dtrace-modules-shared-headers
Requires:     %{name}%{?_isa} = %{version}-%{release}
Group:	      Development/System

%description devel
Headers and libraries to develop DTrace applications.

You do not need this package merely to compile providers and probe points into
applications that will be probed by dtrace, but rather when developing
replacements for dtrace(1) itself.

# We turn off dependency generation for the testsuite because it contains
# test shared libraries of its own, which are otherwise picked up as
# (erroneous) deps to nonexistent packages.
%package testsuite
Summary:      DTrace testsuite.
Requires:     make glibc-devel(%{__isa_name}-32) libgcc(%{__isa_name}-32) dtrace-modules-shared-headers module-init-tools
Requires:     %{name}%{?_isa} = %{version}-%{release}
Autoreq:      0
Group:	      Internal/do-not-release

%description testsuite
The DTrace testsuite.

Installed in /usr/lib64/dtrace/testsuite.

'make check' here is just like 'make check' in the source tree, except that
it always tests the installed DTrace.

%prep
%setup -q

%build
make -j $(getconf _NPROCESSORS_ONLN) VERSION=%{version} KERNELDIRPREFIX=/usr/src/kernels KERNELDIRSUFFIX= KERNELS="$( ( cd /usr/src/kernels; for ver in %{srcdirexp}; do printf "%s " $ver; done) )"
strip --strip-unneeded build/dtrace build/libdtrace.so*

# Force off debuginfo splitting.  We have no debuginfo in dtrace proper,
# and the testsuite requires debuginfo for proper operation.
%global debug_package %{nil}
%global __debug_package %{nil}
%global __spec_install_post \
	%{_rpmconfigdir}/brp-compress \
	%{nil}

%install
mkdir -p $RPM_BUILD_ROOT/usr/sbin
make DESTDIR=$RPM_BUILD_ROOT VERSION=%{version} KERNELDIRPREFIX=/usr/src/kernels KERNELDIRSUFFIX= KERNELS="$( ( cd /usr/src/kernels; for ver in %{srcdirexp}; do printf "%s " $ver; done) )" install install-test
# Because systemtap creates a dtrace.1 manpage we have to rename
# ours and then shift theirs out of the way (since the systemtap
# dtrace page references a non-existent binary)
mv $RPM_BUILD_ROOT/usr/share/man/man1/dtrace.1 \
   $RPM_BUILD_ROOT/usr/share/man/man1/orcl-dtrace.1

# The same is true of sdt.h.
mv $RPM_BUILD_ROOT/usr/include/sys/sdt.h \
   $RPM_BUILD_ROOT/usr/include/sys/sdt-dtrace.h

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"
rm -rf $RPM_BUILD_DIR/%{name}-%{version}

%post
/sbin/ldconfig
# if systemtap-dtrace.1.gz doesn't exist then we can move the existing dtrace manpage
MANDIR=/usr/share/man/man1
if [ -e $MANDIR/dtrace.1.gz -a ! -e $MANDIR/systemtap-dtrace.1.gz ]; then
    mv $MANDIR/dtrace.1.gz $MANDIR/systemtap-dtrace.1.gz
    ln -s $MANDIR/orcl-dtrace.1.gz $MANDIR/dtrace.1.gz
elif [ ! -e $MANDIR/dtrace.1.gz ]; then
    ln -s $MANDIR/orcl-dtrace.1.gz $MANDIR/dtrace.1.gz
fi

# likewise for sdt.h
SYSINCDIR=/usr/include/sys
if [ -e $SYSINCDIR/sdt.h -a ! -e $SYSINCDIR/sdt-systemtap.h ]; then
    mv $SYSINCDIR/sdt.h $SYSINCDIR/sdt-systemtap.h
    ln -s $SYSINCDIR/sdt-dtrace.h $SYSINCDIR/sdt.h
elif [ ! -e $SYSINCDIR/sdt.h ]; then
    ln -s $SYSINCDIR/sdt-dtrace.h $SYSINCDIR/sdt.h
fi

%postun
/sbin/ldconfig
MANDIR=/usr/share/man/man1
if [ -h $MANDIR/dtrace.1.gz ]; then
    rm -f $MANDIR/dtrace.1.gz
fi

%files
%defattr(-,root,root,-)
%{_libdir}/dtrace
%exclude %{_libdir}/dtrace/testsuite
%{_libdir}/libdtrace.so.*
%{_sbindir}/dtrace
%{_mandir}/man1/orcl-dtrace.1.gz
%{_includedir}/sys/dtrace.h
%{_includedir}/sys/dtrace_types.h
%{_includedir}/linux/dtrace_cpu_defines.h
%{_includedir}/sys/sdt-dtrace.h
%doc %{_docdir}/dtrace-%{version}/*

%files devel
%defattr(-,root,root,-)
%{_bindir}/ctf_module_dump
%{_libdir}/libdtrace.so
%{_includedir}/dtrace.h

%files testsuite
%defattr(-,root,root,-)
%{_libdir}/dtrace/testsuite

%changelog
* Tue Jun 23 2015 - <nick.alcock@oracle.com> - 0.4.6-3
- Fix deadlocks and failures to latch processes for symbol lookup caused
  by failure to correctly track their state over time, in 0.4.6-1+ only.

* Mon Jun 22 2015 - <nick.alcock@oracle.com> - 0.4.6-2
- Released to QA team only
- Fix a rare race causing stalls of fork()ed children of traced processes
  under load, in 0.4.6-1 only [Orabug: 21284447]

* Thu Jun 18 2015 - <nick.alcock@oracle.com> - 0.4.6-1
- Released to QA team only
- Support multiple kernels with a single userspace tree, loading system
  D libraries from directories named like /usr/lib64/dtrace/3.8.
  [Orabug: 21279908]
- Processes being userspace-traced can now receive SIGTRAP.
  [Orabug: 21279300]
- dtrace-utils-devel now depends on the same version of dtrace-utils.
  [Orabug: 21280259]
- No longer lose track of processes that exec() while their dynamic linker
  state is being inspected. [Orabug: 21279300]
- No longer assume that the symbol table of processes that are no longer
  being monitored is unchanged since it was last inspected. [Orabug: 21279300]
- Properly remove breakpoints from fork()ed children. [Orabug: 21279300]

* Mon Feb 16 2015 - <nick.alcock@oracle.com> - 0.4.5-3
- The dependencies are adjusted to pick up the renamed dtrace headers package.
  [Orabug: 20508087]

* Tue Nov 18 2014 - <nick.alcock@oracle.com> - 0.4.5-2
- A number of crashes when out of memory are fixed. [Orabug: 20014606]

* Fri Oct 23 2014 - <nick.alcock@oracle.com> - 0.4.5-1
- Automatically load provider modules from /etc/dtrace-modules, if present
  [Orabug: 19821254]
- Fix intermittent crash on failure of initial grabs or creations of processes
  via dtrace -c, -p, or u*() functions [Orabug: 19679998]
- Reliably track and compensate for processes undergoing execve()s
  [Orabug: 19046684]
- Handle processes hit by stopping signals correctly [Orabug: 18674244]
- Fix a sign-extension bug in breakpoint instruction poking [Orabug: 18674244]
- Fix some broken tests (Kris Van Hees) [Orabug: 19616155]
- Robustify DTrace against changes to glibc's internal data structures
  [Orabug: 19882050]
- Fix DIF subr names in dtrace -S output [Orabug: 19881997]

* Tue Jul 22 2014 - <nick.alcock@oracle.com> - 0.4.4-2
- Ensure that the DOF ELF object does not require execstack
  (Kris Van Hees) [Orabug: 19217436]

* Tue Jul  8 2014 - <nick.alcock@oracle.com> - 0.4.4-1
- New -xcppargs option as part of fixes for the testsuite on OL7
  [Orabug: 19054052]

* Tue May 13 2014 - <nick.alcock@oracle.com> - 0.4.3-1
- Fix array underrun when no textual mapping for the executable can be found
  [Orabug: 18550863]
- Fix unlikely buffer overrun at process-map-read time [Orabug: 18550863]
- Fix traversal of realloc()ed pointer which could lead to textual mappings
  being spuriously missed [Orabug: 18550863]
- Fix error-path dereference of uninitialized variable in error message
  [Orabug: 18550863]

* Thu May  1 2014 - <nick.alcock@oracle.com> - 0.4.2-2
- Interrupting dtrace with a SIGINT while monitored processes are dying no
  longer hangs dtrace on a condition variable [Orabug: 18689795]
- Symbol lookups on processes that died at the same instant now always fail
  and no longer access freed memory [Orabug: 18550863]

* Wed Apr 16 2014 - <nick.alcock@oracle.com> - 0.4.2-1
- killing dtrace while a ustack() is in progress no longer risks killing
  crucial system daemons [Orabug: 18600515]
- Fix a leak of filehandles to executables [Orabug: 18600594]
- Fix ustack() of multithreaded processes [Orabug: 18412802]
- Get the pid and ppid right for multithreaded processes [Orabug: 18412802]
- Fix an uninitialized memory read looking up certain kernel symbols
  [Orabug: 18603463]
- Fixes for newer versions of make, ld, and bison [Orabug: 18551552]

* Tue Jan  7 2014 - <nick.alcock@oracle.com> - 0.4.1-1
- Install showUSDT in docdir. (Kris van Hees) [Orabug: 17968414]
- Install ctf_module_dump. [Orabug: 17968381]
- A lexer bug was fixed causing spurious errors if D scripts contained a
  pragma or comment at intervals of 8192 characters, and preventing
  the use of scripts >16KiB entirely. [Orabug: 17742866]
- Fix devinfo_t's dev_statname and dev_pathanme for cases where the
  device does not have partitions. (Kris van Hees) [Orabug: 17973698]
- A variety of memory leaks and uninitialized memory reads are fixed.
  [Orabug: 17743019]
- Improve drti.o to minimize overhead when DTrace is not running.
  [Orabug: 17973604]
- Emit errors from drti.o on stderr, not stdout. [Orabug: 17973604]
- Use O_CLOEXEC when opening files in drti.o. [Orabug: 17973604]
- Fix RPM dependencies; automatically install and modprobe the dtrace
  modules as needed. [Orabug: 17804881]

* Wed Oct 16 2013 Nick Alcock <nick.alcock@oracle.com> - 0.4.0-8
- Fix format of RPM changelog
- Add missing RPM changelog entries

* Wed Oct 16 2013 Nick Alcock <nick.alcock@oracle.com> - 0.4.0-7
- never released, necessary for release management

* Wed Oct 16 2013 Nick Alcock <nick.alcock@oracle.com> - 0.4.0-6
- Fix visibility of .SUNW_dof sections in dtrace -G object files.
  (Kris van Hees) [Orabug: 17476663]
- Fix typos in changelog and specfile copyright date

* Tue Sep 17 2013 Nick Alcock <nick.alcock@oracle.com> - 0.4.0-5
- avoid deadlocking when doing process operations during dtrace -l.
  [Orabug: 17442388]

* Fri Aug 16 2013 Kris van Hees <kris.van.hees@oracle.com> - 0.4.0-4
- Support for USDT in shared libraries.

* Fri Aug 16 2013 Nick Alcock <nick.alcock@oracle.com> - 0.4.0-3
- never released, necessary for release management

* Fri Aug 16 2013 Kris van Hees <kris.van.hees@oracle.com> - 0.4.0-2
- never released, necessary for release management

* Tue Jul 23 2013 Nick Alcock <nick.alcock@oracle.com> - 0.4.0-1
- ustack() support and symbol lookups.
- USDT support.  dtrace -G works.
- evaltime option now works.
- DTrace headers largely moved to dtrace-modules-headers.
- DTRACE_OPT_* environment variables now set options.
  DTRACE_DEBUG=signal emits debugging output on SIGUSR1 receipt.

* Fri Aug 31 2012 Nick Alcock <nick.alcock@oracle.com> - 0.3.0-1
- CTF support.
- Fixed install path for dtrace libraries.
- Fixed -c and -p options.
- Faster startup.
- Split out a -devel package.

* Mon Mar 19 2012 Nick Alcock <nick.alcock@oracle.com> - 0.2.5-2
- Call ldconfig at appropriate times.

* Tue Mar 13 2012 Nick Alcock <nick.alcock@oracle.com> - 0.2.5
- libdtrace is now a shared library, with non-stable API/ABI.

* Thu Feb 16 2012 Nick Alcock <nick.alcock@oracle.com> - 0.2.4
- Updated README; new NEWS and PROBLEMS; synch with module version

* Thu Feb  9 2012 Nick Alcock <nick.alcock@oracle.com> - 0.2.3
- Fixes for reproducibility of test results under load
- Fix -G when setting the syslibdir

* Mon Feb  6 2012 Nick Alcock <nick.alcock@oracle.com> - 0.2.2
- Fix spurious failures of tst.resize*.d.

* Tue Jan 31 2012 Nick Alcock <nick.alcock@oracle.com> - 0.2.1
- Fix 'make check-installed' with an unbuilt source tree.

* Thu Jan 26 2012 Kris van Hees <kris.van.hees@oracle.com> - 0.2.0
- Branch for 0.2.0 release.

* Fri Oct  7 2011 Philip Copeland <philip.copeland@oracle.com> - 0.1-0.3
- The systemtap package in the 'wild' creates a dtrace.1 manpage
  which is bizarre since it doesn't have an associated dtrace 
  binary. This will cause a conflict and the rpm will not install
  Since that man page is superfluous I've had to add a %post
  section here to move our manpage into position in such a way as
  to keep the rpm database happy. Technically this is a bit evil.

* Tue Oct 06 2011 Nick Alcock <nick.alcock@oracle.com> - 0.2
- Fix copyright.

* Tue Sep 27 2011 Nick Alcock <nick.alcock@oracle.com> - 0.1
- Branch for initial release.

* Mon Jun 27 2011 Pearly Zhao <pearly.zhao@oracle.com> - 0.0.1
- Initial build for dtrace.  
