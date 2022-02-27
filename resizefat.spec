# $Id: resizefat.spec 3 2011-09-14 03:17:55Z efalk $

%define	ver	1.0
%define	rel	1

Name: resizefat
Version: %ver
Release: %rel
Summary: Utility to resize a FAT filesystem
URL: http://resizefat.sourceforge.net
License: GPLv2+
Group: Applications/System
#Source: http://resizefat.sourceforge.net/resizefat-%{PACKAGE_VERSION}.tar.gz
Source: resizefat-%{PACKAGE_VERSION}.tar.gz
BuildRoot: %{_tmppath}/resizefat-root
Prefix: /usr

%description
A simple utility to resize FAT filesystems, based on the parted
library.  This tool allows you to resize FAT filesystems in
cases where you can't access the partition table, where there
are no partitions, or where the partition is part of LVM.

%prep
%setup

%build
make prefix=%prefix

%install
rm -rf $RPM_BUILD_ROOT
make prefix=$RPM_BUILD_ROOT/%prefix install

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%prefix/bin/*
%prefix/share/man/man8/*
