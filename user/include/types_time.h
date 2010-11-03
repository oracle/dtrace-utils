#ifndef _TYPES_TIME_H
#define _TYPES_TIME_H

#include <sys/types32.h>

typedef unsigned long long hrtime_t;

struct itimerval32 {
 	struct	timeval32 it_interval;
 	struct	timeval32 it_value;
};


#endif
