## Spec file for l2tpd
## Layer 2 Tunnelling Protocol Daemon (RFC 2661)

Summary:        Layer 2 Tunnelling Protocol Daemon (RFC 2661)
Name:		l2tpd
Version:	0.69
Vendor:		Jeff McAdams <jeffm@iglou.com>
# Original packagers: Lenny Cartier <lenny@mandrakesoft.com>
# and Per Øyvind Karlsen <peroyvind@delonic.no>
Packager:	Jacco de Leeuw <jacco2@dds.nl>
Release:	10jdl
License:	GPL
Url:		http://www.%{name}.org
Group:		System/Servers

%define		freeswanconfigpath freeswan
%define		openswanconfigpath openswan
%define		strongswanconfigpath strongswan
%define		ipsecconfigpath ipsec.d

# Are you building on SuSE?
# Set here (1=yes 0=no):
%define		suse 0

# I do not convert the upstream tarball from .gz to .bz2 because l2tpd
# is a remote access daemon and security conscious people might want to
# verify its signature against tampering.
Source0:	http://www.%{name}.org/downloads/%{name}-%{version}.tar.gz
Source1:	http://www.%{name}.org/downloads/%{name}-%{version}.tar.gz.sig
Source2:	%{name}.init
Source3:	%{name}-RPM.README
# Minimal sample configuration file for use with FreeS/WAN
Source4:	%{name}.conf
Source5:	%{name}-options.l2tpd
Source11:	%{name}-chapsecrets.sample
# Four sample FreeS/WAN configuration files
Source6:	%{name}-L2TP-PSK.conf
Source7:	%{name}-L2TP-CERT.conf
Source8:	%{name}-L2TP-PSK-orgWIN2KXP.conf
Source9:	%{name}-L2TP-CERT-orgWIN2KXP.conf
# /etc/rc.d init script for SuSE by Bernhard Thoni
Source10:	%{name}-suse.init
# SysV style pty allocation patch from Debian (modified)
# (http://www.l2tpd.org/patches/pty.patch)
Patch0:		%{name}-pty.patch.bz2
# Close stdin for daemon mode (http://www.l2tpd.org/patches/close.patch)
Patch1:		%{name}-close.patch.bz2
# Patch which changes the paths from /etc/l2tp/ to /etc/l2tpd/
Patch2:		%{name}-cfgpath.patch.bz2
# Alternative pty patch by Chris Wilson
Patch4:		%{name}-pty.patch2.bz2
# pty patch by Damion de Soto which prevents "Error 737" loopback errors
Patch5:		%{name}-pty-noecho.patch.bz2
# Patch to get rid of /usr/include warnings on Red Hat, some log() warnings,
# fix a syntax error and an uninitialised variable. Sorry about the remaining
# log() warnings but there are too many of them to fix right now.
Patch6:		%{name}-warnings.patch.bz2
# Patch which adds the "listen-addr" parameter
Patch7:		%{name}-listenaddr.patch.bz2
# Workaround for the 'Specify your hostname' problem with the MSL2TP client
Patch8:		%{name}-MSL2TP-hostname.patch.bz2
# Seems to fix a disconnect problem with the MSL2TP client
Patch9:		%{name}-MSL2TP-StopCCN.patch.bz2
# Fixes a buffer overflow in control.c:write_packet()
Patch10:	%{name}-bufoverflow.patch.bz2

BuildRoot:	%{_tmppath}/%{name}-%{version}-buildroot
Requires:	ppp
%if ! %{suse}
Requires:	initscripts chkconfig
%else
Requires:	aaa_base
%define		_initrddir /etc/init.d
%endif

# rpm-helper is a script by Mandrake which basically does a chkconfig.
# Commented out for compatibility with Red Hat and older Mandrakes.
#PreReq:		rpm-helper

%description
l2tpd is an implementation of the Layer 2 Tunnelling Protocol (RFC 2661).
L2TP allows you to tunnel PPP over UDP. Some ISPs use L2TP to tunnel user
sessions from dial-in servers (modem banks, ADSL DSLAMs) to back-end PPP
servers. Another important application is Virtual Private Networks where
the IPsec protocol is used to secure the L2TP connection (L2TP/IPsec,
RFC 3193). The L2TP/IPsec protocol is mainly used by Windows and 
MacOS X clients. On Linux, l2tpd can be used in combination with IPsec
implementations such as FreeS/WAN, Openswan, Strongswan and KAME.
Example configuration files for such a setup are included in this RPM.

l2tpd works by opening a pseudo-tty for communicating with pppd.
It runs completely in userspace.

Based on the Mandrake RPM by Lenny Cartier <lenny@mandrakesoft.com>
and Per Øyvind Karlsen <peroyvind@delonic.no>
%prep
%setup -q
#%patch0 -p0 -b .debian
# Patch 4 seems better.
%patch4 -p0 -b .strdup-pty
%patch5 -p0 -b .noecho
%patch1 -p1 -b .close-stdin
%patch2 -p0 -b .etcl2tp-path
%patch6 -p0 -b .warnings
%patch7 -p0 -b .listenaddr
%patch8 -p0 -b .hostname
%patch9 -p0 -b .stopconnection
%patch10 -p0 -b .bufoverflow
%if %{suse}
cp -p %{SOURCE10} .
%else
cp -p %{SOURCE2} .
%endif
cp -p %{SOURCE3} RPM.README

%build
# %make is not available on all distributions?
make DFLAGS="$RPM_OPT_FLAGS -g -DDEBUG_PPPD -DDEBUG_CONTROL -DDEBUG_ENTROPY"

%install
function CheckBuildRoot {
    # do a few sanity checks on the BuildRoot
    # to make sure we don't damage a system
    case "${RPM_BUILD_ROOT}" in
        ''|' '|/|/bin|/boot|/dev|/etc|/home|/lib|/mnt|/root|/sbin|/tmp|/usr|/var)
            echo "Yikes!  Don't use '${RPM_BUILD_ROOT}' for a BuildRoot!"
            echo "The BuildRoot gets deleted when this package is rebuilt;"
            echo "something like '/tmp/build-blah' is a better choice."
            return 1
            ;;
        *)  return 0
            ;;
    esac
}
function CleanBuildRoot {
    if CheckBuildRoot; then
        rm -rf "${RPM_BUILD_ROOT}"
    else
        exit 1
    fi
}
CleanBuildRoot

# There's no 'install' rule in the Makefile, so let's do it manually
install -d ${RPM_BUILD_ROOT}%{_sbindir}
install -m755 %{name} ${RPM_BUILD_ROOT}%{_sbindir}
install -d ${RPM_BUILD_ROOT}%{_mandir}/{man5,man8}
install -m644 doc/%{name}.conf.5 ${RPM_BUILD_ROOT}%{_mandir}/man5
install -m644 doc/l2tp-secrets.5 ${RPM_BUILD_ROOT}%{_mandir}/man5/
install -m644 doc/%{name}.8 ${RPM_BUILD_ROOT}%{_mandir}/man8
install -d ${RPM_BUILD_ROOT}%{_sysconfdir}/{%{name},ppp,%{ipsecconfigpath}}
install -m644 doc/%{name}.conf.sample ${RPM_BUILD_ROOT}%{_sysconfdir}/%{name}/
install -m644 %{SOURCE4} ${RPM_BUILD_ROOT}%{_sysconfdir}/%{name}/%{name}.conf
install -m644 %{SOURCE5} ${RPM_BUILD_ROOT}%{_sysconfdir}/ppp/options.l2tpd
install -m644 %{SOURCE6} ${RPM_BUILD_ROOT}%{_sysconfdir}/%{ipsecconfigpath}/L2TP-PSK.conf
install -m644 %{SOURCE7} ${RPM_BUILD_ROOT}%{_sysconfdir}/%{ipsecconfigpath}/L2TP-CERT.conf
install -m644 %{SOURCE8} ${RPM_BUILD_ROOT}%{_sysconfdir}/%{ipsecconfigpath}/L2TP-PSK-orgWIN2KXP.conf
install -m644 %{SOURCE9} ${RPM_BUILD_ROOT}%{_sysconfdir}/%{ipsecconfigpath}/L2TP-CERT-orgWIN2KXP.conf
install -m600 doc/l2tp-secrets.sample ${RPM_BUILD_ROOT}%{_sysconfdir}/%{name}/l2tp-secrets
install -m600 %{SOURCE11} ${RPM_BUILD_ROOT}%{_sysconfdir}/ppp/chap-secrets.sample
install -d ${RPM_BUILD_ROOT}%{_initrddir}
%if %{suse}
install -m755 %{SOURCE10} ${RPM_BUILD_ROOT}%{_initrddir}/%{name}
ln -s ../../%{_initrddir}/%{name} ${RPM_BUILD_ROOT}%{_sbindir}/rc%{name}
%else
install -m755 %{SOURCE2} ${RPM_BUILD_ROOT}%{_initrddir}/%{name}
%endif

%post
# For security reasons l2tpd is not started when the RPM is installed
# or at a reboot. To start l2tpd manually, use:
#   /sbin/service l2tpd start
# To start l2tpd at a reboot, use:
#   /sbin/chkconfig --add l2tpd
#
# The Mandrake RPMs for Openswan, Strongswan and FreeS/WAN do not use
# /etc/ipsec.d/ for some reason. The symbolic links below are a workaround.
# (Probably should test for existence instead of forced creation after making
# a backup copy).
if [ -d %{_sysconfdir}/%{freeswanconfigpath} ]; then
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-PSK.conf %{_sysconfdir}/%{freeswanconfigpath}
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-CERT.conf %{_sysconfdir}/%{freeswanconfigpath}
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-PSK-orgWIN2KXP.conf %{_sysconfdir}/%{freeswanconfigpath}
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-CERT-orgWIN2KXP.conf %{_sysconfdir}/%{freeswanconfigpath}
fi
if [ -d %{_sysconfdir}/%{openswanconfigpath} ]; then
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-PSK.conf %{_sysconfdir}/%{openswanconfigpath}
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-CERT.conf %{_sysconfdir}/%{openswanconfigpath}
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-PSK-orgWIN2KXP.conf %{_sysconfdir}/%{openswanconfigpath}
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-CERT-orgWIN2KXP.conf %{_sysconfdir}/%{openswanconfigpath}
fi
if [ -d %{_sysconfdir}/%{strongswanconfigpath} ]; then
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-PSK.conf %{_sysconfdir}/%{strongswanconfigpath}
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-CERT.conf %{_sysconfdir}/%{strongswanconfigpath}
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-PSK-orgWIN2KXP.conf %{_sysconfdir}/%{strongswanconfigpath}
  ln -sfb %{_sysconfdir}/%{ipsecconfigpath}/L2TP-CERT-orgWIN2KXP.conf %{_sysconfdir}/%{strongswanconfigpath}
fi

%preun
# Don't leave the daemon running at uninstall.
if [ "$1" = 0 ]; then
%if ! %{suse}
  /sbin/service %{name} stop 2>&1 >/dev/null
%else
  /usr/sbin/rcl2tpd stop 2>&1 >/dev/null
%endif
fi
exit 0

%postun
if [ "$1" -ge "1" ]; then
%if ! %{suse}
  /sbin/service %{name} try-restart 2>&1 >/dev/null
%else
  /usr/sbin/rcl2tpd try-restart 2>&1 >/dev/null
%endif
fi

%clean
function CheckBuildRoot {
    # Do a few sanity checks on the BuildRoot
    # to make sure we don't damage a system.
    case "${RPM_BUILD_ROOT}" in
        ''|' '|/|/bin|/boot|/dev|/etc|/home|/lib|/mnt|/root|/sbin|/tmp|/usr|/var)
            echo "Yikes!  Don't use '${RPM_BUILD_ROOT}' for a BuildRoot!"
            echo "The BuildRoot gets deleted when this package is rebuilt;"
            echo "something like '/tmp/build-blah' is a better choice."
            return 1
            ;;
        *)  return 0
            ;;
    esac
}
function CleanBuildRoot {
    if CheckBuildRoot; then
        rm -rf "${RPM_BUILD_ROOT}"
    else
        exit 1
    fi
}
CleanBuildRoot

%files
%defattr(-,root,root)
%doc BUGS CHANGELOG CREDITS LICENSE README TODO doc/rfc2661.txt RPM.README
%{_sbindir}/%{name}
%{_mandir}/*/*
%dir %{_sysconfdir}/%{name}
%config(noreplace) %{_sysconfdir}/%{name}/*
%config(noreplace) %{_sysconfdir}/ppp/*
%dir %{_sysconfdir}/%{ipsecconfigpath}
%config(noreplace) %{_sysconfdir}/%{ipsecconfigpath}/*
%config(noreplace) %{_initrddir}/%{name}
%if %{suse}
%config(noreplace) %{_sbindir}/rc%{name}
%endif

%changelog
* Sun Nov 7 2004 Jacco de Leeuw <jacco2@dds.nl> 0.69-10jdl
- [SECURITY FIX] Added fix from Debian because of a bss-based
  buffer overflow.
  (http://www.mail-archive.com/l2tpd-devel@l2tpd.org/msg01071.html)
- Mandrake's FreeS/WAN, Openswan and Strongswan RPMS use configuration
  directories /etc/{freeswan,openswan,strongswan}. Install our
  configuration files to /etc/ipsec.d and create symbolic links in
  those directories.

* Tue Aug 18 2004 Jacco de Leeuw <jacco2@dds.nl>
- Removed 'leftnexthop=' lines. Not relevant for recent versions
  of FreeS/WAN and derivates.

* Tue Jan 20 2004 Jacco de Leeuw <jacco2@dds.nl>  0.69-9jdl
- Added "noccp" because of too much MPPE/CCP messages sometimes.

* Wed Dec 31 2003 Jacco de Leeuw <jacco2@dds.nl>
- Added patch in order to prevent StopCCN messages.

* Sat Aug 23 2003 Jacco de Leeuw <jacco2@dds.nl>
- MTU/MRU 1410 seems to be the lowest possible for MSL2TP.
  For Windows 2000/XP it doesn't seem to matter.
- Typo in l2tpd.conf (192.168.128/25).

* Fri Aug 8 2003 Jacco de Leeuw <jacco2@dds.nl>  0.69-8jdl
- Added MTU/MRU 1400 to options.l2tpd. I don't know the optimal
  value but some apps had problems with the default value.

* Fri Aug 1 2003 Jacco de Leeuw <jacco2@dds.nl>
- Added workaround for the missing hostname bug in the MSL2TP client
  ('Specify your hostname', error 629: "You have been disconnected
  from the computer you are dialing").

* Thu Jul 20 2003 Jacco de Leeuw <jacco2@dds.nl>  0.69-7jdl
- Added the "listen-addr" global parameter for l2tpd.conf. By
  default, the daemon listens on *all* interfaces. Use
  "listen-addr" if you want it to bind to one specific
  IP address (interface), for security reasons. (See also:
  http://www.jacco2.dds.nl/networking/freeswan-l2tp.html#Firewallwarning)
- Explained in l2tpd.conf that two different IP addresses should be
  used for 'listen-addr' and 'local ip'.
- Modified init script. Upgrades should work better now. You
  still need to start/chkconfig l2tpd manually.
- Renamed the example FreeS/WAN .conf files to better reflect
  the situation. There are two variants using different portselectors.
  Previously I thought Windows 2000/XP used portselector 17/0
  and the rest used 17/1701. But with the release of an updated 
  IPsec client by Microsoft, it turns out that 17/0 must have
  been a mistake: the updated client now also uses 17/1701.

* Mon Apr 10 2003 Jacco de Leeuw <jacco2@dds.nl>  0.69-6jdl
- Changed sample chap-secrets to be valid only for specific
  IP addresses.

* Thu Mar 13 2003 Bernhard Thoni <tech-role@tronicplanet.de>
- Adjustments for SuSE8.x (thanks, Bernhard!)
- Added sample chap-secrets.

* Thu Mar 6 2003 Jacco de Leeuw <jacco2@dds.nl> 0.69-5jdl
- Replaced Dominique's patch by Damion de Soto's, which does not
  depend on the N_HDLC kernel module. 

* Wed Feb 26 2003 Jacco de Leeuw <jacco2@dds.nl> 0.69-4jdl
- Seperate example config files for Win9x (MSL2TP) and Win2K/XP
  due to left/rightprotoport differences.
  Fixing preun for Red Hat.

* Mon Feb 3 2003 Jacco de Leeuw <jacco2@dds.nl> 0.69-3jdl
- Mandrake uses /etc/freeswan/ instead of /etc/ipsec.d/
  Error fixed: source6 was used for both PSK and CERT.

* Wed Jan 29 2003 Jacco de Leeuw <jacco2@dds.nl> 0.69-3jdl
- Added Dominique Cressatti's pty patch in another attempt to
  prevent the Windows 2000 Professional "loopback detected" error.
  Seems to work!

* Wed Dec 25 2002 Jacco de Leeuw <jacco2@dds.nl> 0.69-2jdl
- Added 'connect-delay' to PPP parameters in an attempt to
  prevent the Windows 2000 Professional "loopback detected" error.
  Didn't seem to work.

* Fri Dec 13 2002 Jacco de Leeuw <jacco2@dds.nl> 0.69-1jdl
- Did not build on Red Hat 8.0. Solved by adding comments(?!).
  Bug detected in spec file: chkconfig --list l2tpd does not work
  on Red Hat 8.0. Not important enough to look into yet.

* Sun Nov 17 2002 Jacco de Leeuw <jacco2@dds.nl> 0.69-1jdl
- Tested on Red Hat, required some changes. No gprintf. Used different
  pty patch, otherwise wouldn't run. Added buildroot sanity check.

* Sun Nov 10 2002 Jacco de Leeuw <jacco2@dds.nl>
- Specfile adapted from Mandrake Cooker. The original RPM can be
  retrieved through:
  http://www.rpmfind.net/linux/rpm2html/search.php?query=l2tpd
- Config path changed from /etc/l2tp/ to /etc/l2tpd/ 
  (Seems more logical and rp-l2tp already uses /etc/l2tp/).
- Do not run at boot or install. The original RPM uses a config file
  which is completely commented out, but it still starts l2tpd on all
  interfaces. Could be a security risk. This RPM does not start l2tpd,
  the sysadmin has to edit the config file and start l2tpd explicitly.
- Renamed patches to start with l2tpd-
- Added dependencies for pppd, glibc-devel.
- Use %{name} as much as possible.
- l2tp-secrets contains passwords, thus should not be world readable.
- Removed dependency on rpm-helper.

* Mon Oct 21 2002 Lenny Cartier <lenny@mandrakesoft.com> 0.69-3mdk
- from Per Øyvind Karlsen <peroyvind@delonic.no> :
	- PreReq and Requires
	- Fix preun_service

* Thu Oct 17 2002 Per Øyvind Karlsen <peroyvind@delonic.no> 0.69-2mdk
- Move l2tpd from /usr/bin to /usr/sbin
- Added SysV initscript
- Patch0
- Patch1

* Thu Oct 17 2002 Per Øyvind Karlsen <peroyvind@delonic.no> 0.69-1mdk
- Initial release
