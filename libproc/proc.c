#include <unistd.h>
#include "Pcontrol.h"

pid_t
ps_getpid(struct ps_prochandle *P)
{
	if (P == NULL)
		return -1;
	return (P->pid);
}
