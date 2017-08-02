/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include <fnmatch.h>

int
gmatch(const char *s, const char *p)
{
  return (fnmatch (s, p, 0) == 0);
}
