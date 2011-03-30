#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <mutex.h>
#include <procfs_service.h>

hrtime_t
gethrtime()
{
        struct timespec sp;
        int ret;
        long long v;

        ret = clock_gettime(CLOCK_REALTIME, &sp);
        if (ret)
                return 0;

        v = 1000000000LL;
        v *= sp.tv_sec;
        v += sp.tv_nsec;

        return v;
}

int
pthread_cond_reltimedwait_np(pthread_cond_t *cvp, pthread_mutex_t *mp,
        struct timespec *reltime)
{
  struct timespec ts;
  ts = *reltime;
  ts.tv_sec += time(NULL);
  return pthread_cond_timedwait(cvp, mp, &ts);
}

int
mutex_init(mutex_t *m, int flags1, void *ptr)
{
  return pthread_mutex_init(m, NULL);
}

