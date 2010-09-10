#include <types_time.h>
#include <time.h>
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

