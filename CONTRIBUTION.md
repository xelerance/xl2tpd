# Contributing to xl2tpd

First of, thank you for taking the time to contribute.

*Before spending a lot of time on something, please ask for feedback on your
idea first!* You can ask in the [mailing list](https://lists.openswan.org/cgi-bin/mailman/listinfo/xl2tpd)
or create an [issue](https://github.com/xelerance/xl2tpd/issues).

This project welcomes contribution from the community! Here are a few
suggestions:

* Update the [ipv6 branch](https://github.com/xelerance/xl2tpd/tree/ipv6).
  It needs to be tested and updated (it has diverged from master quite a bit).
* Test and fix up the [libevent branch](https://github.com/xelerance/xl2tpd/tree/libevent).
  There have been reports of crashes. They need to be investigated.  User can
  get more information with the custom  *--debug-signals* and
  *--debug-libevent* option (which is only in this branch)

## **Did you find a bug?**

To report a security issue please send an e-mail to security@xelerance.com

For non-security problems, ensure the bug was not already reported by
searching on GitHub under "[Issues](https://github.com/xelerance/xl2tpd/issues)"
and "[Pull requests](https://github.com/xelerance/xl2tpd/pulls)".

When reporting an issue, please provide output and the content of the logs.

## **Did you write a patch that fixes a bug?**

* Open a new GitHub pull request with the patch.
* Ensure the PR description clearly describes the problem and solution.
  Include the relevant issue number if applicable.
* Always write a clear log message for your commits. One-line messages are
  fine for small changes, but bigger changes should look like this:

    $ git commit -m "A brief summary of the commit
    >
    > A paragraph describing what changed and its impact."

    $ git commit -m "A brief summary of the commit
    >
    > A paragraph describing what changed and its impact."

