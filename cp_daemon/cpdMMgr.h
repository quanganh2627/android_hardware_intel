
#ifndef __CPDMMGR_H__
#define __CPDMMGR_H__
#include "cpd.h"
#include "cpdSystemMonitor.h"

#ifdef MODEM_MANAGER

#define MMGR_CONNECTION_RETRY_TIME_MS 200
#define WAIT_FOR_MMGR_CONNECTION_SEC 5

int cpdStartMMgrMonitor(void);
void cpdStopMMgrMonitor(void);

#else /* !MODEM_MANAGER */

static inline int cpdStartMMgrMonitor(void) { return cpdSystemMonitorStart(); };
static inline void cpdStopMMgrMonitor(void) { return; };

#endif /* MODEM_MANAGER */

#endif /* __CPDMMGR_H__ */
