#include <fnmatch.h>

int
gmatch(const char *s, const char *p)
{
  return (fnmatch (s, p, 0) == 0);
}
