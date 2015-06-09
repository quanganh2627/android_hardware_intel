#ifndef __BACKPORT_LINUX_KTIME_H
#define __BACKPORT_LINUX_KTIME_H
#include_next <linux/ktime.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
#define ktime_to_ms LINUX_BACKPORT(ktime_to_ms)
static inline s64 ktime_to_ms(const ktime_t kt)
{
	struct timeval tv = ktime_to_timeval(kt);
	return (s64) tv.tv_sec * MSEC_PER_SEC + tv.tv_usec / USEC_PER_MSEC;
}
#endif /* #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35) */

#endif
