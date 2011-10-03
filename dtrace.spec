# spec file for package dtrace-utils.
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright 2011 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#

BuildRequires: rpm
Name:         dtrace-utils
License:      CDDL
Group:        Development/Tools
Provides:     dtrace-utils
Requires:     gcc elfutils-libelf
BuildRequires: elfutils-libelf-devel fakeroot
Summary:      Dtrace user interface.
Version:      0.1
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
/usr/share/man/man1/dtrace.1
/usr/include/sys/dtrace.h
/usr/include/sys/ctf.h
/usr/include/sys/ctf_api.h
/usr/share/doc/dtrace-%{version}/*

%changelog
* Tue Sep 27 2011 - nick.alcock@oracle.com - 0.1
- Branch for initial release.
* Mon Jun 27 2011 - pearly.zhao@oracle.com - 0.0.1
- Initial build for dtrace.  
