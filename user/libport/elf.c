#include <libelf.h>
int
elf_getshdrstrndx (Elf *elf, size_t *shstrndx)
{
  if (elf_getshstrndx (elf, shstrndx) == 1)
    return 0;
  else
    return -1;
}

int
elf_getshdrnum(Elf *elf, size_t *shnum)
{
  if (elf_getshnum (elf, shnum) == 1)
    return 0;
  else
    return -1;
}

