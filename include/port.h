#ifndef _PORT_H
#define _PORT_H

#include <pthread.h>
#include <mutex.h>
#include <sys/processor.h>
#include <sys/types.h>

extern size_t strlcpy(char *, const char *, size_t);
extern size_t strlcat(char *, const char *, size_t);

extern int gmatch(const char *s, const char *p);

taskid_t gettaskid(void);
projid_t getprojid(void);
hrtime_t gethrtime(void);

int p_online (processorid_t cpun, int new_status);

int pthread_cond_reltimedwait_np(pthread_cond_t *cvp, pthread_mutex_t *mp,
    struct timespec *reltime);

int mutex_init(mutex_t *m, int flags1, void *ptr);

#endif
