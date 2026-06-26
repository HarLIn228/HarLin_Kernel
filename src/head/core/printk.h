#ifndef HARLIN_PRINTK_H
#define HARLIN_PRINTK_H

#ifndef KBUILD_MODNAME
#define KBUILD_MODNAME "kernel"
#endif

#define KERN_EMERG   "[emerg] "
#define KERN_ALERT   "[alert] "
#define KERN_CRIT    "[crit]  "
#define KERN_ERR     "[err]   "
#define KERN_WARN    "[warn]  "
#define KERN_NOTICE  "[notice]"
#define KERN_INFO    "[info]  "
#define KERN_DEBUG   "[debug] "

#define KERN_EMERG_N 0
#define KERN_ALERT_N 1
#define KERN_CRIT_N  2
#define KERN_ERR_N   3
#define KERN_WARN_N  4
#define KERN_NOTICE_N 5
#define KERN_INFO_N  6
#define KERN_DEBUG_N 7

#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL KERN_INFO_N
#endif

void printk(const char* fmt, ...);

#define pr_fmt_impl(fmt) "[" KBUILD_MODNAME "] " fmt
#define pr_emerg(fmt, ...) do { if (CONFIG_LOG_LEVEL >= KERN_EMERG_N) printk(KERN_EMERG pr_fmt_impl(fmt), ##__VA_ARGS__); } while (0)
#define pr_alert(fmt, ...) do { if (CONFIG_LOG_LEVEL >= KERN_ALERT_N) printk(KERN_ALERT pr_fmt_impl(fmt), ##__VA_ARGS__); } while (0)
#define pr_crit(fmt, ...)  do { if (CONFIG_LOG_LEVEL >= KERN_CRIT_N)  printk(KERN_CRIT  pr_fmt_impl(fmt), ##__VA_ARGS__); } while (0)
#define pr_err(fmt, ...)   do { if (CONFIG_LOG_LEVEL >= KERN_ERR_N)   printk(KERN_ERR   pr_fmt_impl(fmt), ##__VA_ARGS__); } while (0)
#define pr_warn(fmt, ...)  do { if (CONFIG_LOG_LEVEL >= KERN_WARN_N)  printk(KERN_WARN  pr_fmt_impl(fmt), ##__VA_ARGS__); } while (0)
#define pr_notice(fmt,...) do { if (CONFIG_LOG_LEVEL >= KERN_NOTICE_N) printk(KERN_NOTICE pr_fmt_impl(fmt),##__VA_ARGS__); } while (0)
#define pr_info(fmt, ...)  do { if (CONFIG_LOG_LEVEL >= KERN_INFO_N)  printk(KERN_INFO  pr_fmt_impl(fmt), ##__VA_ARGS__); } while (0)
#define pr_debug(fmt, ...) do { if (CONFIG_LOG_LEVEL >= KERN_DEBUG_N) printk(KERN_DEBUG pr_fmt_impl(fmt), ##__VA_ARGS__); } while (0)

#endif
