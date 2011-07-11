#
# spec file for package dtrace-user
#
# Copyright (c) 2010-2011 Oracle Corporation.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

BuildRequires: rpm
Name:         dtrace-user
License:      CDDL
Group:        Development/Tools
Provides:     dtrace-user
Requires:     gcc elfutils-libelf
BuildRequires: elfutils-libelf-devel fakeroot
Summary:      Dtrace user interface.
Version:      0.0.1
Release:      0.1
Source:       dtrace-user-%{version}.tar.bz2
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
make -j $(grep ^processor /proc/cpuinfo | wc -l)

%install
echo rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/sbin
fakeroot make DESTDIR=$RPM_BUILD_ROOT install
cp build-*/dtrace $RPM_BUILD_ROOT/usr/sbin

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"
rm -rf $RPM_BUILD_DIR/%{name}-%{version}

%files
%defattr(-,root,root,755)
%exclude /usr/src/debug
%exclude /usr/lib/debug
/usr/lib/dtrace/*
/usr/sbin/dtrace


%changelog
* Mon Jun 27 2011 - pearly.zhao@oracle.com - 0.0.1
- Initial build for dtrace.  
