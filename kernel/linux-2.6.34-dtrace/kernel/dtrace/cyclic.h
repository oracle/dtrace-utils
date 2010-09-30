#ifndef _CYCLIC_H_
#define _CYCLIC_H_

#include <linux/types.h>

typedef uintptr_t	cyclic_id_t;

#define CYCLIC_NONE	((cyclic_id_t)0)

extern void cyclic_remove(cyclic_id_t);

#endif /* _CYCLIC_H_ */
