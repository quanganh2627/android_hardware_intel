#ifndef _COMPAT_LINUX_PRINTK_H
#define _COMPAT_LINUX_PRINTK_H 1

#include <linux/version.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36))
#include_next <linux/printk.h>
#else
#include <linux/kernel.h>
#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)) */

/* see pr_fmt at end of file */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
/* backports 7a555613 */
#if defined(CONFIG_DYNAMIC_DEBUG)
#define dynamic_hex_dump(prefix_str, prefix_type, rowsize,     \
			 groupsize, buf, len, ascii)            \
do {                                                           \
	DEFINE_DYNAMIC_DEBUG_METADATA(descriptor,               \
	__builtin_constant_p(prefix_str) ? prefix_str : "hexdump");\
	if (unlikely(descriptor.flags & _DPRINTK_FLAGS_PRINT))  \
		print_hex_dump(KERN_DEBUG, prefix_str,          \
			       prefix_type, rowsize, groupsize, \
			       buf, len, ascii);                \
} while (0)
#define print_hex_dump_debug(prefix_str, prefix_type, rowsize, \
			     groupsize, buf, len, ascii)        \
	dynamic_hex_dump(prefix_str, prefix_type, rowsize,      \
			 groupsize, buf, len, ascii)
#else
#define print_hex_dump_debug(prefix_str, prefix_type, rowsize,         \
			     groupsize, buf, len, ascii)                \
	print_hex_dump(KERN_DEBUG, prefix_str, prefix_type, rowsize,    \
		       groupsize, buf, len, ascii)
#endif /* defined(CONFIG_DYNAMIC_DEBUG) */

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0) */

#ifndef pr_warn
#define pr_warn pr_warning
#endif

#ifndef printk_once
#define printk_once(x...) ({			\
	static bool __print_once;		\
						\
	if (!__print_once) {			\
		__print_once = true;		\
		printk(x);			\
	}					\
})
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
#define pr_emerg_once(fmt, ...)					\
	printk_once(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert_once(fmt, ...)					\
	printk_once(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit_once(fmt, ...)					\
	printk_once(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err_once(fmt, ...)					\
	printk_once(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn_once(fmt, ...)					\
	printk_once(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_notice_once(fmt, ...)				\
	printk_once(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info_once(fmt, ...)					\
	printk_once(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont_once(fmt, ...)					\
	printk_once(KERN_CONT pr_fmt(fmt), ##__VA_ARGS__)
#if defined(DEBUG)
#define pr_debug_once(fmt, ...)					\
	printk_once(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_debug_once(fmt, ...)					\
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
/* mask va_format as RHEL6 backports this */
#define va_format LINUX_BACKPORT(va_format)

struct va_format {
	const char *fmt;
	va_list *va;
};

/*
 * Dummy printk for disabled debugging statements to use whilst maintaining
 * gcc's format and side-effect checking.
 */
/* mask no_printk as RHEL6 backports this */
#define no_printk LINUX_BACKPORT(no_printk)
static inline __attribute__ ((format (printf, 1, 2)))
int no_printk(const char *s, ...) { return 0; }
#endif

#ifndef printk_ratelimited
/*
 * ratelimited messages with local ratelimit_state,
 * no local ratelimit_state used in the !PRINTK case
 */
#ifdef CONFIG_PRINTK
#define printk_ratelimited(fmt, ...)					\
({									\
	static DEFINE_RATELIMIT_STATE(_rs,				\
				      DEFAULT_RATELIMIT_INTERVAL,	\
				      DEFAULT_RATELIMIT_BURST);		\
									\
	if (__ratelimit(&_rs))						\
		printk(fmt, ##__VA_ARGS__);				\
})
#else
#define printk_ratelimited(fmt, ...)					\
	no_printk(fmt, ##__VA_ARGS__)
#endif
#endif /* printk_ratelimited */

#ifndef pr_emerg_ratelimited
#define pr_emerg_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#endif /* pr_emerg_ratelimited */

#ifndef pr_alert_ratelimited
#define pr_alert_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#endif /* pr_alert_ratelimited */

#ifndef pr_crit_ratelimited
#define pr_crit_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#endif /* pr_crit_ratelimited */

#ifndef pr_err_ratelimited
#define pr_err_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#endif /* pr_err_ratelimited */

#ifndef pr_warn_ratelimited
#define pr_warn_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#endif /* pr_warn_ratelimited */

#ifndef pr_notice_ratelimited
#define pr_notice_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#endif /* pr_notice_ratelimited */

#ifndef pr_info_ratelimited
#define pr_info_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif /* pr_info_ratelimited */

/* no pr_cont_ratelimited, don't do that... */

#ifndef pr_devel_ratelimited
#if defined(DEBUG)
#define pr_devel_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_devel_ratelimited(fmt, ...)					\
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif
#endif /* pr_devel_ratelimited */

#ifndef pr_debug_ratelimited
/* If you are writing a driver, please use dev_dbg instead */
#if defined(CONFIG_DYNAMIC_DEBUG)
/* descriptor check is first to prevent flooding with "callbacks suppressed" */
#define pr_debug_ratelimited(fmt, ...)					\
do {									\
	static DEFINE_RATELIMIT_STATE(_rs,				\
				      DEFAULT_RATELIMIT_INTERVAL,	\
				      DEFAULT_RATELIMIT_BURST);		\
	DEFINE_DYNAMIC_DEBUG_METADATA(descriptor, fmt);			\
	if (unlikely(descriptor.flags & _DPRINTK_FLAGS_PRINT) &&	\
	    __ratelimit(&_rs))						\
		__dynamic_pr_debug(&descriptor, fmt, ##__VA_ARGS__);	\
} while (0)
#elif defined(DEBUG)
#define pr_debug_ratelimited(fmt, ...)					\
	printk_ratelimited(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_debug_ratelimited(fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif
#endif /* pr_debug_ratelimited */

#endif	/* _COMPAT_LINUX_PRINTK_H */

/* This must be outside -- see also kernel.h */
#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif
