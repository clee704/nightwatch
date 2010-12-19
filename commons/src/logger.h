#include <syslog.h>

#define SYSLOG(level, format, args...) \
    syslog(level, LOGGER_PREFIX format, ## args)

#define CRITICAL(format, args...) SYSLOG(LOG_CRIT, format, ## args)
#define ERROR(format, args...) SYSLOG(LOG_ERR, format, ## args)
#define WARNING(format, args...) SYSLOG(LOG_WARNING, format, ## args)
#define INFO(format, args...) SYSLOG(LOG_INFO, format, ## args)
#define DEBUG(format, args...) SYSLOG(LOG_DEBUG, format, ## args)
