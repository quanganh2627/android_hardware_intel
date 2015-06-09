#ifndef __BACKPORT_LINUX_PHY_H
#define __BACKPORT_LINUX_PHY_H
#include_next <linux/phy.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
#define phy_connect(dev, bus_id, handler, interface) \
	phy_connect(dev, bus_id, handler, 0, interface)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
#include <linux/mii.h>
static inline int backport_phy_mii_ioctl(struct phy_device *phydev,
					 struct ifreq *ifr, int cmd)
{
	return phy_mii_ioctl(phydev, if_mii(ifr), cmd);
}
#define phy_mii_ioctl LINUX_BACKPORT(phy_mii_ioctl)
#endif

#endif /* __BACKPORT_LINUX_PHY_H */
