#ifndef _BACKPORT_LINUX_IF_H
#define _BACKPORT_LINUX_IF_H
#include_next <linux/if.h>
#include <linux/version.h>

/* mask IFF_DONT_BRIDGE as RHEL6 backports this */
#if !defined(IFF_DONT_BRIDGE)
#define IFF_DONT_BRIDGE 0x800		/* disallow bridging this ether dev */
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
#define br_port_exists(dev)	(dev->br_port)
#else
/*
 * This is not part of The 2.6.37 kernel yet but we
 * we use it to optimize the backport code we
 * need to implement. Instead of using ifdefs
 * to check what version of the check we use
 * we just replace all checks on current code
 * with this. I'll submit this upstream too, that
 * way all we'd have to do is to implement this
 * for older kernels, then we would not have to
 * edit the upstrema code for backport efforts.
 */
#define br_port_exists(dev)	(dev->priv_flags & IFF_BRIDGE_PORT)
#endif

#ifndef  IFF_TX_SKB_SHARING
#define IFF_TX_SKB_SHARING	0x10000
#endif

#ifndef IFF_LIVE_ADDR_CHANGE
#define IFF_LIVE_ADDR_CHANGE 0x100000
#endif

#ifndef IFF_SUPP_NOFCS
#define IFF_SUPP_NOFCS	0x80000		/* device supports sending custom FCS */
#endif

#ifndef IFF_UNICAST_FLT
#define IFF_UNICAST_FLT	0x20000		/* Supports unicast filtering	*/
#endif

#endif	/* _BACKPORT_LINUX_IF_H */
