# xl2tpd

[![Build Status](https://travis-ci.org/xelerance/xl2tpd.svg?branch=1.3.16dev)](https://travis-ci.org/xelerance/xl2tpd)

xl2tpd is an implementation of the Layer 2 Tunneling Protocol
as defined by [RFC 2661](https://tools.ietf.org/rfc/rfc2661.txt).
L2TP allows you to tunnel PPP over UDP. Some ISPs use L2TP to tunnel user
sessions from dial-in servers (modem banks, ADSL DSLAMs) to back-end PPP
servers. Another important application is Virtual Private Networks where
the IPsec protocol is used to secure the L2TP connection (L2TP/IPsec is
defined by [RFC 3193](https://tools.ietf.org/rfc/rfc3193.txt). xl2tpd can
be used in combination with IPsec implementations such as Openswan. Example
configuration files for such a setup are included in the examples directory.

xl2tpd uses a pseudo-tty to communicate with pppd.
It runs in userspace but supports kernel mode L2TP.

xl2tpd supports IPsec SA Reference tracking to enable overlapping internal
NAT'ed IP's by different clients (eg all clients connecting from their
linksys internal IP 192.168.1.101) as well as multiple clients behind
the same NAT router.

Xl2tpd is based on the L2TP code base of Jeff McAdams <jeffm@iglou.com>.
It was de-facto maintained by Jacco de Leeuw <jacco2@dds.nl> in 2002 and 2003.

NOTE: In Linux kernel 4.15+ there is a kernel bug with ancillary IP_PKTINFO.
      As such, for Linux kernel 4.15+ we recommend the community use xl2tpd
      1.3.12+

## Build and install
    make
    sudo make install

The xl2tpd.conf(5) man page has details on how to configure xl2tpd.


## Mailing Lists

https://lists.openswan.org/cgi-bin/mailman/listinfo/xl2tpd
is home of the mailing list.

Note: This is a closed list - you **must** be subscribed to be able
to post mails.

## Security Vulnerability

Security vulnerabilities can be e-mailed to: security@xelerance.com
