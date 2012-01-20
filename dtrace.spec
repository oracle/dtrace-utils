# spec file for package dtrace-utils.
#
# Copyright 2011 Oracle, Inc.  All rights reserved.
#

BuildRequires: rpm
Name:         dtrace-utils
License:      Oracle Corporation
Group:        Development/Tools
Provides:     dtrace-utils
Requires:     gcc elfutils-libelf
BuildRequires: elfutils-libelf-devel kernel-headers glibc-headers fakeroot byacc flex zlib-devel
Summary:      Dtrace user interface.
Version:      0.2
Release:      0.1
Source:       dtrace-utils-%{version}.tar.bz2
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
BuildArch:    x86_64

%description
Dtrace user interface

 Authors:
--------
Pearly Zhao <pearly.zhao@oracle.com>
Nick Alcock <nick.alcock@oracle.com>

%prep
%setup -q

%build
make -j $(getconf _NPROCESSORS_ONLN) VERSION=%{version}

%install
echo rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/sbin
fakeroot make DESTDIR=$RPM_BUILD_ROOT VERSION=%{version} install

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"
rm -rf $RPM_BUILD_DIR/%{name}-%{version}

%files
%defattr(-,root,root,755)
%exclude /usr/src/debug
%exclude /usr/lib/debug
/usr/lib/dtrace/*
/usr/sbin/dtrace
/usr/share/man/man1/dtrace.1.gz
/usr/include/sys/dtrace.h
/usr/include/sys/dtrace_types.h
/usr/include/sys/ctf.h
/usr/include/sys/ctf_api.h
/usr/share/doc/dtrace-%{version}/*

%changelog
* Web Jan 18 2012 - nick.alcock@oracle.com - 0.2
- Second release, adding SDT support.
* Tue Sep 27 2011 - nick.alcock@oracle.com - 0.1
- Branch for initial release.
* Mon Jun 27 2011 - pearly.zhao@oracle.com - 0.0.1
- Initial build for dtrace.  
