#ifndef _GMATCH_H
#define _GMATCH_H
#include <fnmatch.h>

int
gmatch(const char *s, const char *p)
{
  return (fnmatch (s, p, 0) == 0);
}

#endif
