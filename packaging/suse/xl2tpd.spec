#
# spec file for package xl2tpd
#
# Copyright (c) 2019 SUSE LINUX GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#


%if 0%{?suse_version} <= 1310
%define rundir %{_localstatedir}/run
%else
%define rundir /run
%endif
Name:           xl2tpd
Version: 1.3.18
Release:        0
Summary:        Layer 2 Tunnelling Protocol Daemon (RFC 2661)
License:        GPL-2.0-only
Group:          Productivity/Networking/System
URL:            http://www.xelerance.com/software/xl2tpd/
Source0:        https://github.com/xelerance/xl2tpd/archive/v%{version}.tar.gz
Source1:        %{name}.service
Source2:        %{name}.conf
Patch0:         Makefile.patch
Patch1:         xl2tpd.init.patch
BuildRequires:  libpcap
BuildRequires:  libpcap-devel
BuildRequires:  linux-kernel-headers >= 2.6.19
BuildRequires:  systemd-rpm-macros
Requires:       ppp
Obsoletes:      l2tpd <= 0.68
Provides:       l2tpd = 0.69
%{?systemd_ordering}

%description
xl2tpd is an implementation of the Layer 2 Tunnelling Protocol (RFC 2661).
L2TP allows you to tunnel PPP over UDP. Some ISPs use L2TP to tunnel user
sessions from dial-in servers (modem banks, ADSL DSLAMs) to back-end PPP
servers. Another important application is Virtual Private Networks where
the IPsec protocol is used to secure the L2TP connection (L2TP/IPsec,
RFC 3193). The L2TP/IPsec protocol is mainly used by Windows and
Mac OS X clients. On Linux, xl2tpd can be used in combination with IPsec
implementations such as Openswan.
Example configuration files for such a setup are included in this RPM.

xl2tpd works by opening a pseudo-tty for communicating with pppd.
It runs completely in userspace but supports kernel mode L2TP.

xl2tpd supports IPsec SA Reference tracking to enable overlapping internak
NAT'ed IP's by different clients (eg all clients connecting from their
linksys internal IP 192.168.1.101) as well as multiple clients behind
the same NAT router.

xl2tpd supports the pppol2tp kernel mode operations on 2.6.23 or higher,
or via a patch in contrib for 2.4.x kernels.

Xl2tpd is based on the 0.69 L2TP by Jeff McAdams <jeffm@iglou.com>
It was de-facto maintained by Jacco de Leeuw <jacco2@dds.nl> in 2002 and 2003.

%prep
%setup -q
%patch0
%patch1

%build
make %{?_smp_mflags} DFLAGS="%{optflags} -D_GNU_SOURCE $(getconf LFS_CFLAGS)"

%install
export PREFIX=%{_prefix}
%make_install
install -p -D -m644 examples/xl2tpd.conf %{buildroot}%{_sysconfdir}/xl2tpd/xl2tpd.conf
install -p -d -m750 %{buildroot}%{_sysconfdir}/ppp
install -p -D -m644 examples/ppp-options.xl2tpd %{buildroot}%{_sysconfdir}/ppp/options.xl2tpd
install -p -D -m600 doc/l2tp-secrets.sample %{buildroot}%{_sysconfdir}/xl2tpd/l2tp-secrets
install -p -D -m600 examples/chapsecrets.sample %{buildroot}%{_sysconfdir}/ppp/chap-secrets.sample
install -p -D -m755 -d %{buildroot}%{rundir}/xl2tpd
install -D -m0644 %{SOURCE1} %{buildroot}%{_unitdir}/%{name}.service
install -D -m0644 %{SOURCE2} %{buildroot}%{_tmpfilesdir}/%{name}.conf
%if 0%{?suse_version} > 1310
sed -i 's|%{_localstatedir}/run/|/run/|' %{buildroot}%{_tmpfilesdir}/%{name}.conf
%endif
mkdir -p %{buildroot}%{_prefix}/lib/modules-load.d
echo "l2tp_ppp" > %{buildroot}%{_prefix}/lib/modules-load.d/%{name}.conf
ln -s %{_sbindir}/service %{buildroot}%{_sbindir}/rc%{name}

%pre
%service_add_pre %{name}.service

%post
# if we migrate from l2tpd to xl2tpd, copy the configs
if [ -f %{_sysconfdir}/l2tpd/l2tpd.conf ]
then
	echo "Old %{_sysconfdir}/l2tpd configuration found, migrating to %{_sysconfdir}/xl2tpd"
	mv %{_sysconfdir}/xl2tpd/xl2tpd.conf %{_sysconfdir}/xl2tpd/xl2tpd.conf.rpmsave
	cat %{_sysconfdir}/l2tpd/l2tpd.conf | sed "s/options.l2tpd/options.xl2tpd/" > %{_sysconfdir}/xl2tpd/xl2tpd.conf
	mv %{_sysconfdir}/ppp/options.xl2tpd %{_sysconfdir}/ppp/options.xl2tpd.rpmsave
	mv %{_sysconfdir}/ppp/options.l2tpd %{_sysconfdir}/ppp/options.xl2tpd
	mv %{_sysconfdir}/xl2tpd/l2tp-secrets %{_sysconfdir}/xl2tpd/l2tpd-secrets.rpmsave
	cp -pa %{_sysconfdir}/l2tpd/l2tp-secrets %{_sysconfdir}/xl2tpd/l2tp-secrets

fi

%service_add_post %{name}.service
%fillup_only
%tmpfiles_create %{_tmpfilesdir}/%{name}.conf

%preun
%service_del_preun %{name}.service

%postun
%service_del_postun %{name}.service

%files
%license LICENSE
%doc BUGS CHANGES CREDITS README.* TODO
%doc doc/README.patents examples/chapsecrets.sample
%{_sbindir}/rcxl2tpd
%{_sbindir}/xl2tpd
%{_sbindir}/xl2tpd-control
%{_bindir}/pfc
%dir %{_sysconfdir}/xl2tpd
%config(noreplace) %{_sysconfdir}/xl2tpd/*
%dir %{_sysconfdir}/ppp
%config(noreplace) %{_sysconfdir}/ppp/*
%dir %ghost %{rundir}/xl2tpd
%ghost %{rundir}/xl2tpd/l2tp-control
%{_tmpfilesdir}/%{name}.conf
%{_unitdir}/%{name}.service
%dir %{_prefix}/lib/modules-load.d
%{_prefix}/lib/modules-load.d/%{name}.conf
%{_mandir}/man1/pfc.1%{?ext_man}
%{_mandir}/man5/l2tp-secrets.5%{?ext_man}
%{_mandir}/man5/xl2tpd.conf.5%{?ext_man}
%{_mandir}/man8/xl2tpd-control.8%{?ext_man}
%{_mandir}/man8/xl2tpd.8%{?ext_man}

%changelog
